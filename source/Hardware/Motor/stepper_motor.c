#include "Hardware/Motor/42Motor.h"
#include "Hardware/Motor/stepper_motor.h"
#include "Hardware/Buzzer&Light/B&L.h"
#include "ti_msp_dl_config.h"

#include <limits.h>

volatile bool if_return_balance = false;
volatile bool if_recore_balance = false;
volatile bool if_ball_open_loop_start = false;
volatile bool if_lift_down_test = false;

extern volatile uint8_t RunFlag;

typedef enum
{
    RX_WAIT_ADDR = 0,
    RX_WAIT_CMD,
    RX_COLLECT,
} LiftRxState;

typedef struct
{
    LiftRxState state;
    uint8_t buf[8];
    uint8_t index;
    uint8_t expected_length;
} LiftRxContext;

static volatile LiftMotorState g_lift_state = {0};
static LiftRxContext g_rx = {RX_WAIT_ADDR, {0}, 0, 0};
static uint16_t g_motion_rpm = LIFT_MOTOR_DEFAULT_RPM;
static uint8_t g_motion_acc = LIFT_MOTOR_DEFAULT_ACC;
static uint8_t g_next_query_is_flags = 0U;
static volatile LiftMotorStartupState g_startup_state =
    LIFT_STARTUP_NOT_INITIALIZED;
static uint16_t g_status_request_count = 0U;
static uint32_t g_enable_status_baseline = 0U;
static volatile uint32_t g_lift_tick_10ms = 0U;
static volatile LiftBallTaskState g_ball_task_state =
    LIFT_BALL_TASK_IDLE;
static volatile LiftBallTaskResult g_ball_task_result =
    LIFT_BALL_RESULT_NONE;
static uint32_t g_ball_task_deadline = 0U;
/* 临时硬件标定入口：J-Link写入非零脉冲后由主循环执行，标定完成后删除。 */
volatile int32_t g_lift_debug_delta_pulse = 0;

#define LIFT_MS_TO_10MS_TICKS(ms) (((ms) + 9U) / 10U)
#define LIFT_FLAG_ARRIVED         0x02U

static void lift_set_startup_state(LiftMotorStartupState state)
{
    g_startup_state = state;
    g_lift_state.startup_state = (uint8_t)state;
}

static void lift_set_ball_task_state(LiftBallTaskState state)
{
    g_ball_task_state = state;
    g_lift_state.ball_task_state = (uint8_t)state;
}

static void lift_set_ball_task_result(LiftBallTaskResult result)
{
    g_ball_task_result = result;
    g_lift_state.ball_task_result = (uint8_t)result;
}

static bool lift_tick_due(uint32_t deadline)
{
    return ((int32_t)(g_lift_tick_10ms - deadline) >= 0);
}

static void lift_reset_status_request_count(void)
{
    g_status_request_count = 0U;
    g_lift_state.status_request_count = 0U;
}

static void lift_request_status(void)
{
    Emm_V5_Read_Sys_Params(LIFT_MOTOR_ADDR, S_FLAG, UART_2_INST);
    if (g_status_request_count < UINT16_MAX)
    {
        g_status_request_count++;
        g_lift_state.status_request_count = g_status_request_count;
    }
}

static uint32_t pulse_magnitude(int32_t pulse)
{
    if (pulse >= 0)
    {
        return (uint32_t)pulse;
    }

    return (uint32_t)(-(int64_t)pulse);
}

static uint8_t pulse_direction(int32_t pulse)
{
    uint8_t direction = (pulse >= 0) ? 0U : 1U;

#if LIFT_MOTOR_DIRECTION_INVERT
    direction ^= 1U;
#endif

    return direction;
}

static uint32_t bytes_be_u32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

static uint8_t expected_packet_length(uint8_t command)
{
    switch (command)
    {
        case 0x36:
            return 8U; /* S_CPOS: addr+cmd+sign+4B+0x6B */

        case 0x3A:
            return 4U; /* S_FLAG: addr+cmd+flags+0x6B */

        default:
            return 0U;
    }
}

static void dispatch_packet(const uint8_t *packet)
{
    if (packet[1] == 0x36)
    {
        uint32_t magnitude = bytes_be_u32(&packet[3]);
        if (magnitude <= (uint32_t)INT32_MAX)
        {
            int32_t signed_position = (int32_t)magnitude;
            g_lift_state.position_count = packet[2] ? -signed_position : signed_position;
            g_lift_state.position_update_count++;
        }
    }
    else if (packet[1] == 0x3A)
    {
        g_lift_state.flags = packet[2];
        g_lift_state.flags_update_count++;
    }
}

void LiftMotor_Init(void)
{
    if (g_lift_state.initialized != 0U)
    {
        return;
    }

    /* 清空 RX FIFO 残留数据，防止上电瞬间的噪声字节卡住状态机 */
    while (!DL_UART_Main_isRXFIFOEmpty(UART_2_INST))
    {
        (void)DL_UART_Main_receiveData(UART_2_INST);
    }
    DL_UART_Main_clearInterruptStatus(UART_2_INST, DL_UART_MAIN_INTERRUPT_RX);
    DL_UART_Main_enableInterrupt(UART_2_INST, DL_UART_MAIN_INTERRUPT_RX);
    NVIC_ClearPendingIRQ(UART_2_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_2_INST_INT_IRQN);

    g_lift_state.initialized = 1U;
    lift_set_startup_state(LIFT_STARTUP_WAIT_STATUS);
    lift_reset_status_request_count();
    Light_OFF();

    /* 先读取状态，不在尚未确认通信时盲目重初始化或使能。 */
    lift_request_status();
}

void LiftMotor_Enable(bool enable)
{
    if (g_lift_state.initialized == 0U)
    {
        return;
    }

    Emm_V5_En_Control(LIFT_MOTOR_ADDR, enable, false, UART_2_INST);
}

void LiftMotor_SetProfile(uint16_t rpm, uint8_t acc)
{
    if (rpm == 0U)
    {
        rpm = 1U;
    }
    else if (rpm > LIFT_MOTOR_MAX_RPM)
    {
        rpm = LIFT_MOTOR_MAX_RPM;
    }

    g_motion_rpm = rpm;
    g_motion_acc = acc;
}

int32_t Height_Trans(float height)
{
    float pulse = height * (float)LIFT_MOTOR_PULSES_PER_REV / LIFT_SCREW_LEAD_MM;

    if (pulse >= 0.0f)
    {
        return (int32_t)(pulse + 0.5f);
    }

    return (int32_t)(pulse - 0.5f);
}

bool LiftMotor_Ctrl(float height)
{
    return LiftMotor_MoveRelativePulse(Height_Trans(height));
}

bool LiftMotor_MoveRelativePulse(int32_t delta_pulse)
{
    int64_t next_command;

    if ((g_lift_state.initialized == 0U) ||
        (g_startup_state != LIFT_STARTUP_READY) ||
        (delta_pulse == 0))
    {
        return false;
    }

    next_command = (int64_t)g_lift_state.command_pulse + delta_pulse;
    if ((next_command > INT32_MAX) || (next_command < INT32_MIN))
    {
        return false;
    }

    Emm_V5_Pos_Control(LIFT_MOTOR_ADDR,
                       pulse_direction(delta_pulse),
                       g_motion_rpm,
                       g_motion_acc,
                       pulse_magnitude(delta_pulse),
                       false,
                       false,
                       UART_2_INST);
    g_lift_state.command_pulse = (int32_t)next_command;
    return true;
}

bool LiftMotor_MoveAbsolutePulse(int32_t target_pulse)
{
    if ((g_lift_state.initialized == 0U) ||
        (g_startup_state != LIFT_STARTUP_READY) ||
        (g_lift_state.balance_valid == 0U))
    {
        return false;
    }

    Emm_V5_Pos_Control(LIFT_MOTOR_ADDR,
                       pulse_direction(target_pulse),
                       g_motion_rpm,
                       g_motion_acc,
                       pulse_magnitude(target_pulse),
                       true,
                       false,
                       UART_2_INST);
    g_lift_state.command_pulse = target_pulse;
    return true;
}

void LiftMotor_Stop(void)
{
    if (g_lift_state.initialized != 0U)
    {
        Emm_V5_Stop_Now(LIFT_MOTOR_ADDR, false, UART_2_INST);
    }
}

bool LiftMotor_RecordBalanceHere(void)
{
    if (g_lift_state.initialized == 0U)
    {
        return false;
    }

    Emm_V5_Reset_CurPos_To_Zero(LIFT_MOTOR_ADDR, UART_2_INST);
    g_lift_state.command_pulse = 0;
    g_lift_state.balance_valid = 1U;
    return true;
}

void LiftMotor_RecordBalance(void)
{
    (void)LiftMotor_RecordBalanceHere();
}

bool LiftMotor_BalanceDetect(void)
{
    /* 当前没有物理原点传感器，只表示本次上电是否已人工记录平衡点。 */
    return (g_lift_state.balance_valid != 0U);
}

bool LiftMotor_ReturnToBalance(void)
{
    if (g_lift_state.balance_valid == 0U)
    {
        return false;
    }

    return LiftMotor_MoveAbsolutePulse(0);
}

void LiftMotor_Tick10ms(void)
{
    g_lift_tick_10ms++;
}

static bool lift_ball_task_start(void)
{
    int32_t plus_target;

    if ((g_ball_task_state != LIFT_BALL_TASK_IDLE) ||
        (g_startup_state != LIFT_STARTUP_READY) ||
        (RunFlag != 0U) ||
        (g_lift_state.balance_valid == 0U) ||
        (g_lift_state.command_pulse != 0) ||
        ((g_lift_state.flags & LIFT_FLAG_ARRIVED) == 0U))
    {
        lift_set_ball_task_result(LIFT_BALL_RESULT_REJECTED);
        return false;
    }

    /*
     * 当前 +4mm 会让机构向下、小球向 -5cm 运动；
     * 因此第一步使用绝对 -4mm，让机构反向倾斜并使小球先去 +5cm。
     */
    plus_target = Height_Trans(-LIFT_BALL_TILT_MM);
    if (!LiftMotor_MoveAbsolutePulse(plus_target))
    {
        lift_set_ball_task_result(LIFT_BALL_RESULT_COMMAND_FAILED);
        return false;
    }

    g_ball_task_deadline =
        g_lift_tick_10ms +
        LIFT_MS_TO_10MS_TICKS(LIFT_BALL_TO_PLUS_TIME_MS);
    lift_set_ball_task_state(LIFT_BALL_TASK_TO_PLUS);
    lift_set_ball_task_result(LIFT_BALL_RESULT_RUNNING);
    return true;
}

static void lift_ball_task_fail(void)
{
    (void)LiftMotor_MoveAbsolutePulse(0);
    lift_set_ball_task_state(LIFT_BALL_TASK_IDLE);
    lift_set_ball_task_result(LIFT_BALL_RESULT_COMMAND_FAILED);
}

static void lift_ball_task_process(void)
{
    switch (g_ball_task_state)
    {
        case LIFT_BALL_TASK_TO_PLUS:
            if (lift_tick_due(g_ball_task_deadline))
            {
                int32_t minus_target = Height_Trans(LIFT_BALL_TILT_MM);

                if (!LiftMotor_MoveAbsolutePulse(minus_target))
                {
                    lift_ball_task_fail();
                    return;
                }

                g_ball_task_deadline =
                    g_lift_tick_10ms +
                    LIFT_MS_TO_10MS_TICKS(
                        LIFT_BALL_TO_MINUS_TIME_MS);
                lift_set_ball_task_state(LIFT_BALL_TASK_TO_MINUS);
            }
            break;

        case LIFT_BALL_TASK_TO_MINUS:
            if (lift_tick_due(g_ball_task_deadline))
            {
                int32_t brake_target =
                    Height_Trans(-LIFT_BALL_BRAKE_MM);

                /*
                 * 小球正向 -5cm 运动时，短暂反向倾斜产生制动力，
                 * 先消除负方向速度，再恢复水平。
                 */
                if (!LiftMotor_MoveAbsolutePulse(brake_target))
                {
                    lift_ball_task_fail();
                    return;
                }

                g_ball_task_deadline =
                    g_lift_tick_10ms +
                    LIFT_MS_TO_10MS_TICKS(
                        LIFT_BALL_BRAKE_TIME_MS);
                lift_set_ball_task_state(LIFT_BALL_TASK_BRAKE);
            }
            break;

        case LIFT_BALL_TASK_BRAKE:
            if (lift_tick_due(g_ball_task_deadline))
            {
                if (!LiftMotor_MoveAbsolutePulse(0))
                {
                    lift_ball_task_fail();
                    return;
                }

                g_ball_task_deadline =
                    g_lift_tick_10ms +
                    LIFT_MS_TO_10MS_TICKS(
                        LIFT_BALL_RETURN_SETTLE_MS);
                lift_set_ball_task_state(
                    LIFT_BALL_TASK_RETURN_ZERO);
            }
            break;

        case LIFT_BALL_TASK_RETURN_ZERO:
            if (lift_tick_due(g_ball_task_deadline))
            {
                lift_set_ball_task_state(LIFT_BALL_TASK_IDLE);
                lift_set_ball_task_result(
                    LIFT_BALL_RESULT_COMPLETED);
            }
            break;

        case LIFT_BALL_TASK_IDLE:
        default:
            break;
    }
}

void LiftMotor_Task(void)
{
    int32_t debug_delta = g_lift_debug_delta_pulse;

    if (g_startup_state == LIFT_STARTUP_WAIT_STATUS)
    {
        if (g_lift_state.flags_update_count == 0U)
        {
            return;
        }

        if ((g_lift_state.flags & 0x01U) != 0U)
        {
            lift_set_startup_state(LIFT_STARTUP_READY);
            Light_ON();
        }
        else
        {
            g_enable_status_baseline = g_lift_state.flags_update_count;
            Emm_V5_En_Control(
                LIFT_MOTOR_ADDR, true, false, UART_2_INST);
            lift_reset_status_request_count();
            lift_set_startup_state(LIFT_STARTUP_ENABLE_SENT);
        }
        return;
    }

    if (g_startup_state == LIFT_STARTUP_ENABLE_SENT)
    {
        if (g_lift_state.flags_update_count <= g_enable_status_baseline)
        {
            return;
        }

        if ((g_lift_state.flags & 0x01U) != 0U)
        {
            lift_set_startup_state(LIFT_STARTUP_READY);
            Light_ON();
        }
        else if (g_status_request_count >= LIFT_MOTOR_STATUS_MAX_REQUESTS)
        {
            lift_set_startup_state(LIFT_STARTUP_ENABLE_FAILED);
        }
        return;
    }

    if (g_startup_state != LIFT_STARTUP_READY)
    {
        return;
    }

    if (g_ball_task_state != LIFT_BALL_TASK_IDLE)
    {
        /* 任务运行期间忽略重复中键和重新记录请求。右键仍可安全取消并回零。 */
        if_ball_open_loop_start = false;
        if_recore_balance = false;
        if_lift_down_test = false;
        g_lift_debug_delta_pulse = 0;

        if (if_return_balance)
        {
            if_return_balance = false;
            (void)LiftMotor_ReturnToBalance();
            lift_set_ball_task_state(LIFT_BALL_TASK_IDLE);
            lift_set_ball_task_result(LIFT_BALL_RESULT_CANCELED);
            return;
        }

        lift_ball_task_process();
        return;
    }

    if (debug_delta != 0)
    {
        g_lift_debug_delta_pulse = 0;
        (void)LiftMotor_MoveRelativePulse(debug_delta);
        return;
    }

    if (if_recore_balance)
    {
        if_recore_balance = false;
        g_lift_state.last_action = 1U;
        g_lift_state.action_count++;
        LiftMotor_RecordBalance();
        return;
    }

    if (if_return_balance)
    {
        if_return_balance = false;
        g_lift_state.last_action = 2U;
        g_lift_state.action_count++;
        (void)LiftMotor_ReturnToBalance();
        return;
    }

    if (if_ball_open_loop_start)
    {
        if_ball_open_loop_start = false;
        g_lift_state.last_action = 3U;
        g_lift_state.action_count++;
        (void)lift_ball_task_start();
        return;
    }

    if (if_lift_down_test)
    {
        if_lift_down_test = false;
        g_lift_state.last_action = 4U;
        g_lift_state.action_count++;
        (void)LiftMotor_Ctrl(-LIFT_MOTOR_BUTTON_STEP_MM);
    }
}

void LiftMotor_RequestState(void)
{
    if (g_lift_state.initialized == 0U)
    {
        return;
    }

    if ((g_startup_state == LIFT_STARTUP_WAIT_STATUS) ||
        (g_startup_state == LIFT_STARTUP_ENABLE_SENT))
    {
        if (g_status_request_count >= LIFT_MOTOR_STATUS_MAX_REQUESTS)
        {
            if (g_lift_state.flags_update_count == 0U)
            {
                lift_set_startup_state(
                    (g_lift_state.rx_byte_count == 0U) ?
                    LIFT_STARTUP_NO_RX : LIFT_STARTUP_INVALID_RX);
            }
            else if (g_startup_state == LIFT_STARTUP_ENABLE_SENT)
            {
                lift_set_startup_state(LIFT_STARTUP_ENABLE_FAILED);
            }
            return;
        }

        lift_request_status();
        return;
    }

    if (g_startup_state != LIFT_STARTUP_READY)
    {
        return;
    }

    if (g_next_query_is_flags == 0U)
    {
        Emm_V5_Read_Sys_Params(LIFT_MOTOR_ADDR, S_CPOS, UART_2_INST);
        g_next_query_is_flags = 1U;
    }
    else
    {
        Emm_V5_Read_Sys_Params(LIFT_MOTOR_ADDR, S_FLAG, UART_2_INST);
        g_next_query_is_flags = 0U;
    }
}

void LiftMotor_GetState(LiftMotorState *state)
{
    if (state == NULL)
    {
        return;
    }

    state->command_pulse = g_lift_state.command_pulse;
    state->position_count = g_lift_state.position_count;
    state->flags = g_lift_state.flags;
    state->initialized = g_lift_state.initialized;
    state->balance_valid = g_lift_state.balance_valid;
    state->command_rpm = g_motion_rpm;
    state->command_acc = g_motion_acc;
    state->position_update_count = g_lift_state.position_update_count;
    state->flags_update_count = g_lift_state.flags_update_count;
    state->rx_byte_count = g_lift_state.rx_byte_count;
    state->action_count = g_lift_state.action_count;
    state->last_rx_byte = g_lift_state.last_rx_byte;
    state->last_action = g_lift_state.last_action;
    state->status_request_count = g_status_request_count;
    state->startup_state = (uint8_t)g_startup_state;
    state->ball_task_state = (uint8_t)g_ball_task_state;
    state->ball_task_result = (uint8_t)g_ball_task_result;
}

void UART2_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_2_INST))
    {
        case DL_UART_MAIN_IIDX_RX:
            while (!DL_UART_Main_isRXFIFOEmpty(UART_2_INST))
            {
                uint8_t byte = DL_UART_Main_receiveData(UART_2_INST);
                g_lift_state.last_rx_byte = byte;
                g_lift_state.rx_byte_count++;

                switch (g_rx.state)
                {
                    case RX_WAIT_ADDR:
                        if (byte == LIFT_MOTOR_ADDR)
                        {
                            g_rx.buf[0] = byte;
                            g_rx.index = 1U;
                            g_rx.state = RX_WAIT_CMD;
                        }
                        break;

                    case RX_WAIT_CMD:
                        g_rx.buf[g_rx.index++] = byte;
                        g_rx.expected_length = expected_packet_length(byte);
                        if (g_rx.expected_length == 0U)
                        {
                            g_rx.state = RX_WAIT_ADDR;
                            g_rx.index = 0U;
                        }
                        else
                        {
                            g_rx.state = RX_COLLECT;
                        }
                        break;

                    case RX_COLLECT:
                        if (g_rx.index < (uint8_t)sizeof(g_rx.buf))
                        {
                            g_rx.buf[g_rx.index++] = byte;
                        }
                        else
                        {
                            g_rx.state = RX_WAIT_ADDR;
                            g_rx.index = 0U;
                            break;
                        }

                        if (g_rx.index >= g_rx.expected_length)
                        {
                            if (g_rx.buf[g_rx.index - 1U] == 0x6B)
                            {
                                dispatch_packet(g_rx.buf);
                            }

                            g_rx.state = RX_WAIT_ADDR;
                            g_rx.index = 0U;
                        }
                        break;

                    default:
                        g_rx.state = RX_WAIT_ADDR;
                        g_rx.index = 0U;
                        break;
                }
            }
            break;

        default:
            break;
    }
}
