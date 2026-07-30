#ifndef __STEPPER_MOTOR_H
#define __STEPPER_MOTOR_H
/*严禁删注释,有错可以修改但是得提醒我,可以添加必要的简洁注释*/
#include <stdbool.h>
#include <stdint.h>

//丝杆长度100mm,12导程；每度位移=12/360=0.03333mm，命令精度由驱动器每转脉冲数决定
#define LIFT_SCREW_LEAD_MM              12.0f
#define LIFT_MOTOR_PULSES_PER_REV       3200U

#define LIFT_MOTOR_BUTTON_STEP_MM       4.0f
#define LIFT_BALL_TILT_MM               4.0f
#define LIFT_BALL_TO_PLUS_TIME_MS       1100U
#define LIFT_BALL_TO_MINUS_TIME_MS      900U
#define LIFT_BALL_BRAKE_MM              4.0f
#define LIFT_BALL_BRAKE_TIME_MS         300U
#define LIFT_BALL_RETURN_SETTLE_MS      200U

/* 当前使用临时调试；以后更换电机地址只改这一处。 */
#define LIFT_MOTOR_ADDR                 0x01U

/* Emm_V5硬件规格为3000RPM+；比赛机构在实测前先限制到1000RPM。

   硬件配置参数(我感觉还需要一些参数控制,有必要吗感觉速度很重要)
*/
#define LIFT_MOTOR_DEFAULT_RPM          300U
#define LIFT_MOTOR_MAX_RPM             1000U
#define LIFT_MOTOR_DEFAULT_ACC          100U
#define LIFT_MOTOR_STATUS_MAX_REQUESTS  20U

/* 抬升方向判断标志位 */
#define LIFT_MOTOR_DIRECTION_INVERT      0U

//判断位
extern volatile bool if_return_balance;
extern volatile bool if_recore_balance;
extern volatile bool if_ball_open_loop_start;
extern volatile bool if_lift_down_test;

typedef enum
{
    LIFT_STARTUP_NOT_INITIALIZED = 0,
    LIFT_STARTUP_WAIT_STATUS,
    LIFT_STARTUP_READY,
    LIFT_STARTUP_ENABLE_SENT,
    LIFT_STARTUP_NO_RX,
    LIFT_STARTUP_INVALID_RX,
    LIFT_STARTUP_ENABLE_FAILED,
} LiftMotorStartupState;

typedef enum
{
    LIFT_BALL_TASK_IDLE = 0,
    LIFT_BALL_TASK_TO_PLUS,
    LIFT_BALL_TASK_TO_MINUS,
    LIFT_BALL_TASK_BRAKE,
    LIFT_BALL_TASK_RETURN_ZERO,
} LiftBallTaskState;

typedef enum
{
    LIFT_BALL_RESULT_NONE = 0,
    LIFT_BALL_RESULT_RUNNING,
    LIFT_BALL_RESULT_COMPLETED,
    LIFT_BALL_RESULT_REJECTED,
    LIFT_BALL_RESULT_COMMAND_FAILED,
    LIFT_BALL_RESULT_CANCELED,
} LiftBallTaskResult;

typedef struct
{
    int32_t command_pulse;          /* MCU累计的绝对目标脉冲 */
    int32_t position_count;         /* S_CPOS回传值，65536 count/电机轴一圈 */
    uint8_t flags;                  /* bit0使能、bit1到位、bit2堵转、bit3堵转保护 */
/*还能完善一下,只加入有必要的状态位没必要把低频率使用的也加入,遵循奥卡姆剃刀*/
    uint8_t initialized;
    uint8_t balance_valid;
    uint16_t command_rpm;           /* MCU当前下发速度，不冒充驱动器实测速度 */
    uint8_t command_acc;
    uint32_t position_update_count; /* 用于确认UART位置回包持续更新 */
    uint32_t flags_update_count;    /* 用于确认UART状态回包持续更新 */
    uint32_t rx_byte_count;         /* UART2收到的全部原始字节，不要求协议正确 */
    uint32_t action_count;          /* 主循环实际处理的按键动作次数 */
    uint8_t last_rx_byte;
    uint8_t last_action;            /* 1记录、2恢复、3上升1mm、4下降1mm */
    uint16_t status_request_count;  /* 启动阶段已发送的 S_FLAG 查询次数 */
    uint8_t startup_state;          /* LiftMotorStartupState：通信与使能诊断结果 */
    uint8_t ball_task_state;        /* LiftBallTaskState：开环小球任务阶段 */
    uint8_t ball_task_result;       /* LiftBallTaskResult：最近一次任务结果 */
} LiftMotorState;

/* 只调用一次：开启UART接收并使能单个Emm_V5电机，不自动运动或写零点。 */
void LiftMotor_Init(void);
//调试用,读取运动速度 rpm 是否初始化 转向 地址等参数设置和状态等方便ai和我调试,你来完善一下
void LiftMotor_GetState(LiftMotorState *state);

void LiftMotor_Enable(bool enable);
void LiftMotor_SetProfile(uint16_t rpm, uint8_t acc);

/* 本层只使用原始脉冲；丝杆导程和抬升高度换算放在后续上层模块。 */
bool LiftMotor_MoveRelativePulse(int32_t delta_pulse);
/* 绝对位置必须先调用LiftMotor_RecordBalanceHere建立本次上电的零点。 */
bool LiftMotor_MoveAbsolutePulse(int32_t target_pulse);
void LiftMotor_Stop(void);

/* 原点标定和检测(掉电也得记录与另一边5mm平衡的位置,有必要可以重记录矫正保证快速归位)。 */
bool LiftMotor_BalanceDetect(void);
void LiftMotor_RecordBalance();//目前我要调试这个你给两个按键加一个记录和归位
bool LiftMotor_RecordBalanceHere(void);
bool LiftMotor_ReturnToBalance(void);

//加入换算内容和抬升函数,我输入抬升高度->换算函数和抬升执行
int32_t Height_Trans(float height);
bool LiftMotor_Ctrl(float height);

/* 按键中断只置请求位；主循环调用本函数执行UART命令。 */
void LiftMotor_Task(void);
/* 在10ms定时器中断中调用，只累加开环任务时基，不执行UART操作。 */
void LiftMotor_Tick10ms(void);

/* 每次调用交替请求位置和状态，回包由UART2中断异步解析。 */
//懒得看了兄弟,你评估一下会不会阻塞,能不能快速响应,有什么改进空间和遗留问题
void LiftMotor_RequestState(void);

#endif
