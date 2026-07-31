#include "ti_msp_dl_config.h"
#include "Hardware/Board/board.h"
#include "Hardware/LCD/lcd.h"
#include "Hardware/LCD/lcd_init.h"
#include "Hardware/TB6612/bsp_tb6612.h"
#include "Hardware/Encoder/Encoder.h"
#include "middle/mid_debug_led.h"
#include "middle/mid_pid.h"
#include "middle/mid_laptime.h"
#include "app/app_ui.h"
#include "app/app_key_task.h"
#include "Hardware/BlueSerial/bsp_hc05.h"
#include <stdlib.h>
#include "Hardware/jy901s/jy901s.h"
#include "app/app_irtracking.h"
#include "Hardware/Buzzer&Light/B&L.h"
#include "Hardware/Motor/stepper_motor.h"
#include "CTRL/Jetson/serial_ctrl.h"

/*
 * 小球控制几何参数：管子有效长度25cm。
 * 步进电机新驱动层允许+-24mm，PID初调额外限制在更安全的+-5mm。
 */
#define BALL_TUBE_EFFECTIVE_LENGTH_MM   250.0f
#define BALL_PID_MAX_LIFT_MM              5.0f
#define BALL_TUBE_MAX_TILT_DEG             1.1459f
#define BALL_MOTOR_COMMAND_PERIOD_TICKS    4U    /* TIMER_0为10ms，4个tick对应25Hz电机指令 */
#define BALL_MOTOR_MIN_COMMAND_MM          0.10f /* 目标变化小于0.1mm时不重复下发 */

/* 中键任务指标：0 -> +5cm -> -5cm，总时间不得超过5s。 */
#define BALL_TASK_PLUS_TARGET_CM           5.0f
#define BALL_TASK_MINUS_TARGET_CM         -5.0f
#define BALL_TASK_POSITION_TOLERANCE_CM    0.8f  /* 比题目+-1cm收紧0.2cm余量 */
#define BALL_TASK_STABLE_SPEED_CM_S        1.0f
#define BALL_TASK_STABLE_FRAMES            8U    /* 80FPS下约100ms连续稳定 */
#define BALL_TASK_TIMEOUT_10MS_TICKS      500U   /* 500 * 10ms = 5s */

typedef enum
{
    BALL_TASK_IDLE = 0,
    BALL_TASK_TO_PLUS,
    BALL_TASK_TO_MINUS,
    BALL_TASK_HOLD_MINUS,
    BALL_TASK_TIMEOUT,
} BallMotionTaskState_t;

volatile int16_t LeftPWM , RightPWM;
int16_t AvePWM , DifPWM;
volatile uint8_t RunFlag = 0;
volatile uint8_t LineTrackingFlag = 0;
extern volatile uint8_t lap_running;    /* 定义在 mid_laptime.c */
float LeftSpeed , RightSpeed;
float AveSpeed , DifSpeed;
float YawRate;   // Z轴角速度（°/s）
int16_t DeadZone = 0;

/* Jetson小球数据：由UART1中断接收，由TIMER_0每10ms取一次完整新帧。 */
volatile float BallPositionCm = 0.0f;       /* 位置(cm)：中心为0，左负右正 */
volatile float BallVelocityCmS = 0.0f;     /* 速度(cm/s)：向左为负，向右为正 */
volatile uint8_t BallDataFresh = 0U;       /* 新帧标志：1=本次10ms中断读到新帧，0=无新帧 */
volatile uint8_t BallFrameSeq = 0U;        /* Jetson帧序号，用于检查丢帧或重复帧 */
volatile float BallLiftTargetMm = 0.0f;    /* 位置PID换算得到的丝杆绝对高度(mm)，正值升高 */
volatile uint8_t BallBalanceControlEnabled = 0U; /* 0=闭环停止，1=按当前PID目标控制 */
volatile uint8_t BallMotionTaskState = BALL_TASK_IDLE; /* 中键任务当前阶段，供J-Link观察 */
volatile uint16_t BallMotionElapsed10ms = 0U;   /* 启动后的10ms计时，最大500 */
volatile uint16_t BallTurnElapsed10ms = 0U;     /* 首次进入+5cm允许误差带的时间 */
volatile uint8_t BallStableFrameCount = 0U;     /* -5cm附近连续稳定帧数 */

static volatile uint8_t LiftMotorQueryDue = 0U;
static volatile uint8_t LiftMotorFlagsDue = 0U;
static volatile uint8_t BallLiftCommandDue = 0U; /* 由主循环发送UART2电机命令 */

PID_t speed_PID ={
	.Kp = 2000,
	.Ki = 400.0,
	.Kd = 0,

    .OutMax = 7000,
    .OutMin = -7000,

    .ErrorIntMax = 20,
    .ErrorIntMin = -20,
};

PID_t turn_PID ={
	.Kp = 60.0,
	.Ki = 8.5,
	.Kd = 150.0,

	.OutMax = 5000,
	.OutMin = -5000,

    .ErrorIntMax = 420,
    .ErrorIntMin = -420,
};

/*
 * 小球位置外环（位置式PID）：
 * Target/Actual单位为cm，中心为0，左负右正。
 * Out是管子目标倾角(度)，再根据25cm有效长度换算为丝杆升降高度。
 * 初次调试先关闭积分，先调Kp，再增加Kd抑制超调，最后视静差小幅增加Ki。
 */
PID_t ball_position_PID ={
    .Kp = 0.50f,               /* 位置误差每1cm产生约0.046度目标倾角 */
    .Ki = 0.5f,                 /* 初调时禁用积分，防止小球大幅振荡 */
    .Kd = 0.111f,               /* 使用Jetson实测速度作阻尼，抑制超调 */

    .OutMax = BALL_TUBE_MAX_TILT_DEG,  /* +5mm对应的最大正倾角，约+1.146度 */
    .OutMin = -BALL_TUBE_MAX_TILT_DEG, /* -5mm对应的最大负倾角，约-1.146度 */

    .ErrorIntMax = 10.0f,      /* 积分上限，开启Ki后用于抗积分饱和 */
    .ErrorIntMin = -10.0f,     /* 积分下限 */
};

void BallMotionTask_Stop(void)
{
    BallMotionTaskState = BALL_TASK_IDLE;
    BallMotionElapsed10ms = 0U;
    BallTurnElapsed10ms = 0U;
    BallStableFrameCount = 0U;
}

void BallMotionTask_Start(void)
{
    /*
     * 每次按下中键都从第一阶段重新开始；清空PID历史，避免上一次任务的
     * 积分或误差残留影响本次从中心向+5cm起步。
     */
    PID_Init(&ball_position_PID);
    ball_position_PID.Target = BALL_TASK_PLUS_TARGET_CM;
    BallLiftTargetMm = 0.0f;
    BallMotionElapsed10ms = 0U;
    BallTurnElapsed10ms = 0U;
    BallStableFrameCount = 0U;
    BallMotionTaskState = BALL_TASK_TO_PLUS;
    BallBalanceControlEnabled = 1U;
}

static float BallAbsFloat(float value)
{
    return (value >= 0.0f) ? value : -value;
}

static void BallMotionTask_Update(float position_cm, float velocity_cm_s)
{
    if (BallMotionTaskState == BALL_TASK_TO_PLUS)
    {
        /*
         * 题目允许误差为+-1cm，程序收紧为+-0.8cm。小球首次到达+4.2cm即折返，
         * 不等待恰好等于+5cm，以减少惯性造成的正端超调。
         */
        if (position_cm >=
            (BALL_TASK_PLUS_TARGET_CM - BALL_TASK_POSITION_TOLERANCE_CM))
        {
            BallTurnElapsed10ms = BallMotionElapsed10ms;
            BallMotionTaskState = BALL_TASK_TO_MINUS;
            BallStableFrameCount = 0U;

            /* 切换目标时清空PID历史，立即开始向-5cm制动和反向运行。 */
            PID_Init(&ball_position_PID);
            ball_position_PID.Target = BALL_TASK_MINUS_TARGET_CM;
        }
    }
    else if (BallMotionTaskState == BALL_TASK_TO_MINUS)
    {
        bool inside_minus_band =
            (position_cm >=
             (BALL_TASK_MINUS_TARGET_CM - BALL_TASK_POSITION_TOLERANCE_CM)) &&
            (position_cm <=
             (BALL_TASK_MINUS_TARGET_CM + BALL_TASK_POSITION_TOLERANCE_CM));
        bool speed_is_stable =
            (BallAbsFloat(velocity_cm_s) <= BALL_TASK_STABLE_SPEED_CM_S);

        if (inside_minus_band && speed_is_stable)
        {
            if (BallStableFrameCount < BALL_TASK_STABLE_FRAMES)
            {
                BallStableFrameCount++;
            }

            if (BallStableFrameCount >= BALL_TASK_STABLE_FRAMES)
            {
                /* 任务达标后不关闭PID，继续把小球稳定保持在-5cm。 */
                BallMotionTaskState = BALL_TASK_HOLD_MINUS;
            }
        }
        else
        {
            BallStableFrameCount = 0U;
        }
    }
}

/*
 * 管子一端为支点、另一端由丝杆升降，有效长度L=250mm。
 * 几何关系为h=L*sin(theta)；最大倾角仅约1.146度，因此用h≈L*theta避免中断内计算sin。
 */
static float BallPidOutputToLiftHeightMm(float tilt_deg)
{
    float height_mm;

    if (tilt_deg > BALL_TUBE_MAX_TILT_DEG)
    {
        tilt_deg = BALL_TUBE_MAX_TILT_DEG;
    }
    else if (tilt_deg < -BALL_TUBE_MAX_TILT_DEG)
    {
        tilt_deg = -BALL_TUBE_MAX_TILT_DEG;
    }

    /* 角度转弧度后使用小角度近似：h(mm) ≈ L(mm) * theta(rad)。 */
    height_mm = BALL_TUBE_EFFECTIVE_LENGTH_MM *
                tilt_deg * (3.1415926f / 180.0f);

    /* 最终再做一次机械高度限幅，防止浮点误差越界。 */
    if (height_mm > BALL_PID_MAX_LIFT_MM)
    {
        height_mm = BALL_PID_MAX_LIFT_MM;
    }
    else if (height_mm < -BALL_PID_MAX_LIFT_MM)
    {
        height_mm = -BALL_PID_MAX_LIFT_MM;
    }

    return height_mm;
}

static void SystemIrq_Init(void)
{
    NVIC_ClearPendingIRQ(STEP_TIM_INST_INT_IRQN);
    NVIC_EnableIRQ(STEP_TIM_INST_INT_IRQN);

    NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
}

int main(void)
{
    float lift_target_mm;
    LiftMotorState lift_state;
    int32_t target_pulse;
    int32_t delta_pulse;
    int32_t min_command_pulse;
    uint32_t primask;

    SYSCFG_DL_init();
    SystemIrq_Init();
    Encoder_Init();
    TB6612_Motor_Stop();
    LCD_Init();
    user_button_init();
    Bluetooth_Init();
    Gyro_Init();
    BL_Init();
    LiftMotor_Init();
    Serial_Init();
    LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, BLACK);

    while (1)
    {
        Serial_Task();
        LiftMotor_Task();
        LapTime_DisplayTask();

        /*
         * TIMER_0只计算丝杆高度目标，UART2电机命令放在主循环发送，
         * 避免串口发送阻塞10ms控制中断。
         */
        if (BallLiftCommandDue != 0U)
        {
            /* 短临界区确保float目标和请求标志属于同一次PID计算。 */
            primask = __get_PRIMASK();
            __disable_irq();
            lift_target_mm = BallLiftTargetMm;
            BallLiftCommandDue = 0U;
            if (primask == 0U)
            {
                __enable_irq();
            }

            /* 绝对高度基于平衡零点：+5mm升高，-5mm降低，不会逐帧累加。 */
            if (BallBalanceControlEnabled != 0U)
            {
                /*
                 * PID给出相对平衡零点的绝对高度；新版电机接口使用相对位移。
                 * 用新目标减去上一次已下发目标得到本次增量，相同目标不重复发送。
                 */
                LiftMotor_GetState(&lift_state);
                target_pulse = Height_Trans(lift_target_mm);
                delta_pulse = target_pulse - lift_state.command_pulse;
                min_command_pulse =
                    Height_Trans(BALL_MOTOR_MIN_COMMAND_MM);

                /*
                 * 普通调节需超过0.1mm死区才发送，抑制摄像头噪声导致的电机抖动。
                 * 视觉丢失时目标被设为0mm，即使剩余误差小于0.1mm也要精确回到平衡点。
                 */
                if (((lift_target_mm == 0.0f) && (delta_pulse != 0)) ||
                    (delta_pulse >= min_command_pulse) ||
                    (delta_pulse <= -min_command_pulse))
                {
                    if (LiftMotor_MoveRelativePulse(delta_pulse))
                    {
                        /* 给驱动器留出处理运动帧和返回应答的时间，本轮不紧跟查询帧。 */
                        LiftMotorQueryDue = 0U;
                        LiftMotorFlagsDue = 0U;
                    }
                }
            }
        }

        if (LiftMotorFlagsDue != 0U)
        {
            LiftMotorFlagsDue = 0U;
            LiftMotorQueryDue = 0U;
            LiftMotor_RequestFlags();
        }
        else if (LiftMotorQueryDue != 0U)
        {
            LiftMotorQueryDue = 0U;
            LiftMotor_RequestPosition();
        }
    }
}


void TIMER_0_INST_IRQHandler(void)
{
    static uint8_t Count0;
    static uint8_t LiftQueryCount;
    static uint8_t LiftFlagsQueryCount;
    static uint8_t BallMotorCommandTick =
        BALL_MOTOR_COMMAND_PERIOD_TICKS;
    Serial_Data vision_data;

    switch (DL_TimerG_getPendingInterrupt(TIMER_0_INST))
    {
    case DL_TIMER_IIDX_ZERO:
        YawRate = - Gyro_Data.gyro_z;
        flex_button_scan();
        Serial_Tick10ms();
        LapTime_Tick10ms();

        /* PID保持跟随视觉新帧，但UART2电机位置指令最多25Hz。 */
        if (BallMotorCommandTick < BALL_MOTOR_COMMAND_PERIOD_TICKS)
        {
            BallMotorCommandTick++;
        }

        /* 中键任务使用独立10ms时基，确保5s限制不受Jetson帧率波动影响。 */
        if ((BallMotionTaskState == BALL_TASK_TO_PLUS) ||
            (BallMotionTaskState == BALL_TASK_TO_MINUS))
        {
            if (BallMotionElapsed10ms < BALL_TASK_TIMEOUT_10MS_TICKS)
            {
                BallMotionElapsed10ms++;
            }

            if (BallMotionElapsed10ms >= BALL_TASK_TIMEOUT_10MS_TICKS)
            {
                /*
                 * 超时只记录失败状态，PID目标保持-5cm继续控制，
                 * 避免突然回水平导致小球从最终目标处滚走。
                 */
                BallMotionTaskState = BALL_TASK_TIMEOUT;
                ball_position_PID.Target = BALL_TASK_MINUS_TARGET_CM;
                BallStableFrameCount = 0U;
            }
        }

        /*
         * Jetson回传80 FPS，约12.5ms一帧，因此按10ms控制节拍检查新数据。
         * 只有UART1收到一帧完整且CRC正确的数据时，才更新位置和速度；
         * 没有新帧时保留上一帧数值，并将BallDataFresh置0，避免PID重复积分。
         */
        BallDataFresh = 0U;
        if (Serial_TakeFreshData(&vision_data))
        {
            BallPositionCm = vision_data.position_cm;
            BallVelocityCmS = vision_data.velocity_cm_s;
            BallFrameSeq = vision_data.frame_seq;
            BallDataFresh = 1U;

            /*
             * 小球位置外环只在收到Jetson新帧时计算，避免对同一帧重复积分。
             * Jetson回传约80 FPS，名义帧间隔为12.5ms；D项直接使用实测速度抑制超调。
             * ball_position_PID.Out是管子目标倾角(度)，后续换算为丝杆高度。
             */
            if (BallBalanceControlEnabled != 0U)
            {
                if (vision_data.vision_state != VISION_STATE_LOST)
                {
                    /* 先根据当前位置切换+5/-5阶段，再用本帧执行新目标PID。 */
                    BallMotionTask_Update(BallPositionCm, BallVelocityCmS);
                    ball_position_PID.Actual = BallPositionCm;
                    PID_UpdateWithRate(
                        &ball_position_PID,
                        BallVelocityCmS,
                        0.0125f);

                    /* 将摄像头误差得到的倾角估算为-5～+5mm丝杆绝对高度。 */
                    BallLiftTargetMm =
                        BallPidOutputToLiftHeightMm(
                            ball_position_PID.Out);
                }
                else
                {
                    /* 摄像头丢失小球时将管子回到水平位置，禁止无效数据继续控制。 */
                    BallLiftTargetMm = 0.0f;
                }

                if (BallMotorCommandTick >=
                    BALL_MOTOR_COMMAND_PERIOD_TICKS)
                {
                    BallMotorCommandTick = 0U;
                    BallLiftCommandDue = 1U;
                }
            }
        }

        /*
         * UART2查询不能每10ms持续轰炸驱动器，否则会与位置控制帧紧邻发送。
         * 位置20Hz足够观察机械响应；状态位2Hz用于低频健康检查。
         */
        LiftQueryCount++;
        LiftFlagsQueryCount++;
        if (LiftQueryCount >= 5U)
        {
            LiftQueryCount = 0U;
            LiftMotorQueryDue = 1U;
        }
        if (LiftFlagsQueryCount >= 50U)
        {
            LiftFlagsQueryCount = 0U;
            LiftMotorFlagsDue = 1U;
        }

        if(LineTrackingFlag)
        {
            LineWalking();
            turn_PID.Target = LineTracking_GetError();

            /*
             * 黑线=0，白底=1。
             * 起跑500ms屏蔽结束后，T1~T8任意至少6路同时见黑，立即硬停。
             */
            if (LapTime_IsPastStartGuard())
            {
                uint8_t black_count = 0U;
                uint8_t atv1 = DL_GPIO_readPins(Track_T1_PORT, Track_T1_PIN) ? 1U : 0U;
                uint8_t atv2 = DL_GPIO_readPins(Track_T2_PORT, Track_T2_PIN) ? 1U : 0U;
                uint8_t atv3 = DL_GPIO_readPins(Track_T3_PORT, Track_T3_PIN) ? 1U : 0U;
                uint8_t atv4 = DL_GPIO_readPins(Track_T4_PORT, Track_T4_PIN) ? 1U : 0U;
                uint8_t atv5 = DL_GPIO_readPins(Track_T5_PORT, Track_T5_PIN) ? 1U : 0U;
                uint8_t atv6 = DL_GPIO_readPins(Track_T6_PORT, Track_T6_PIN) ? 1U : 0U;
                uint8_t atv7 = DL_GPIO_readPins(Track_T7_PORT, Track_T7_PIN) ? 1U : 0U;
                uint8_t atv8 = DL_GPIO_readPins(Track_T8_PORT, Track_T8_PIN) ? 1U : 0U;

                black_count += (atv1 == 0U) ? 1U : 0U;
                black_count += (atv2 == 0U) ? 1U : 0U;
                black_count += (atv3 == 0U) ? 1U : 0U;
                black_count += (atv4 == 0U) ? 1U : 0U;
                black_count += (atv5 == 0U) ? 1U : 0U;
                black_count += (atv6 == 0U) ? 1U : 0U;
                black_count += (atv7 == 0U) ? 1U : 0U;
                black_count += (atv8 == 0U) ? 1U : 0U;

                if (black_count >= 6U)
                {
                    RunFlag = 0U;
                    LineTrackingFlag = 0U;
                    lap_running = 0U;

                    LeftPWM = 0;
                    RightPWM = 0;
                    AvePWM = 0;
                    DifPWM = 0;
                    PID_Init(&speed_PID);
                    PID_Init(&turn_PID);
                    TB6612_Motor_Stop();
                    LapTime_MarkFinished();
                }
            }
        }
        if(RunFlag)
        {
            turn_PID.Actual = YawRate;
            PID_Update(&turn_PID);
            DifPWM = turn_PID.Out;

            LeftPWM  = AvePWM + DifPWM;
            RightPWM = AvePWM - DifPWM;

            Motor_SetPWM(1 , LeftPWM);
			Motor_SetPWM(2 , RightPWM);
        }
        Count0 ++;
        if(Count0 >= 4)
		{
            Count0 = 0;
            Encoder_UpdateSpeed();
            //转速
            LeftSpeed = Encoder_Get(0) * 25.0f / 780.0f;
            RightSpeed = Encoder_Get(1) * 25.0f / 780.0f;

            AveSpeed = (LeftSpeed + RightSpeed) / 2.0;
            if(RunFlag)
			{
				speed_PID.Actual = AveSpeed;
				PID_Update(&speed_PID);
				AvePWM = speed_PID.Out;

				if (AvePWM > 0) AvePWM += DeadZone;
				else if (AvePWM < 0) AvePWM -= DeadZone;
			}
        }
        break;
    default:
        break;
    }
}
