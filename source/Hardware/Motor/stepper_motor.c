#include "Hardware/Motor/42Motor.h"
#include "Hardware/Motor/stepper_motor.h"
#include "ti_msp_dl_config.h"

#include <limits.h>

volatile bool if_return_balance = false;
volatile bool if_recore_balance = false;
volatile bool if_lift_up_test = false;
volatile bool if_lift_down_test = false;

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
/* 临时硬件标定入口：J-Link写入非零脉冲后由主循环执行，标定完成后删除。 */
volatile int32_t g_lift_debug_delta_pulse = 0;

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

        case 0x0A: /* 当前位置清零应答 */
        case 0xF3: /* 电机使能应答 */
        case 0xFD: /* 位置模式控制应答 */
            return 4U; /* addr+cmd+status+0x6B */

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
    else if ((packet[1] == 0x0A) ||
             (packet[1] == 0xF3) ||
             (packet[1] == 0xFD))
    {
        /* 保存写命令应答，便于区分“MCU已发送”和“驱动器已接受”。 */
        g_lift_state.last_command_reply = packet[1];
        g_lift_state.last_command_status = packet[2];
        g_lift_state.command_reply_count++;
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
    /* 单电机立即使能，snF=false表示不等待多机同步触发。 */
    Emm_V5_En_Control(LIFT_MOTOR_ADDR, true, false, UART_2_INST);
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

    if ((g_lift_state.initialized == 0U) || (delta_pulse == 0))
    {
        return false;
    }

    next_command = (int64_t)g_lift_state.command_pulse + delta_pulse;
    if ((next_command > INT32_MAX) || (next_command < INT32_MIN))
    {
        return false;
    }
    if ((next_command > LIFT_MOTOR_LIMIT_PULSE) ||
        (next_command < -LIFT_MOTOR_LIMIT_PULSE))
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
        (g_lift_state.balance_valid == 0U))
    {
        return false;
    }
    if ((target_pulse > LIFT_MOTOR_LIMIT_PULSE) ||
        (target_pulse < -LIFT_MOTOR_LIMIT_PULSE))
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

void LiftMotor_Task(void)
{
    int32_t debug_delta = g_lift_debug_delta_pulse;

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

    if (if_lift_up_test)
    {
        if_lift_up_test = false;
        g_lift_state.last_action = 3U;
        g_lift_state.action_count++;
        (void)LiftMotor_Ctrl(LIFT_MOTOR_BUTTON_STEP_MM);
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

    if (g_next_query_is_flags == 0U)
    {
        LiftMotor_RequestPosition();
        g_next_query_is_flags = 1U;
    }
    else
    {
        LiftMotor_RequestFlags();
        g_next_query_is_flags = 0U;
    }
}

void LiftMotor_RequestPosition(void)
{
    if (g_lift_state.initialized != 0U)
    {
        Emm_V5_Read_Sys_Params(LIFT_MOTOR_ADDR, S_CPOS, UART_2_INST);
    }
}

void LiftMotor_RequestFlags(void)
{
    if (g_lift_state.initialized != 0U)
    {
        Emm_V5_Read_Sys_Params(LIFT_MOTOR_ADDR, S_FLAG, UART_2_INST);
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
    state->command_reply_count = g_lift_state.command_reply_count;
    state->last_rx_byte = g_lift_state.last_rx_byte;
    state->last_action = g_lift_state.last_action;
    state->last_command_reply = g_lift_state.last_command_reply;
    state->last_command_status = g_lift_state.last_command_status;
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
