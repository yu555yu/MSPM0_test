#include "42Motor.h"
#include "stepper_motor.h"
#include "Hardware/Board/board.h"

// 比赛连续绕圈时 PAN 需要持续跟随；置 0 仅关闭 RelativeControl 的 PAN 软件限幅。
// TILT 相对限幅和 AbsoluteControl 双轴绝对限幅始终保留。
#define PAN_RELATIVE_LIMIT_ENABLE  0

// 全局电机状态（[0]=pan, [1]=tilt），UART2 中断里写、主循环读
volatile MotorState motorState[2] = {0};

volatile uint8_t pose_query_flag  = 0;
volatile uint8_t bias_print_flag  = 0;
volatile uint8_t stepper_config_status = 0;

#define TILT_CONFIG_FRAME_LEN      33u
#define TILT_CONFIG_PARAM_COUNT    21u
#define PAN_CONFIG_FRAME_LEN       37u
#define PAN_CONFIG_PARAM_COUNT     24u
#define DRIVER_CONFIG_DATA_OFFSET   4u
#define TILT_CONFIG_DATA_LEN       28u
#define PAN_CONFIG_DATA_LEN        32u
#define PAN_CONFIG_ID_OFFSET       22u
#define CONFIG_REPLY_TIMEOUT_TICKS 50u   // TIMER_0 为 10ms：等待回包 500ms
#define CONFIG_WRITE_SETTLE_TICKS  20u   // EEPROM 写入后等待 200ms 再读回

extern volatile uint32_t ControlTick10ms;

typedef enum {
    CONFIG_IDLE = 0,
    CONFIG_REQUESTED,
    CONFIG_WAIT_TILT,
    CONFIG_READ_PAN_REQUESTED,
    CONFIG_WAIT_PAN_SOURCE,
    CONFIG_READY_WRITE,
    CONFIG_WRITE_SETTLE,
    CONFIG_VERIFY_REQUESTED,
    CONFIG_WAIT_PAN_VERIFY,
    CONFIG_TEST_READY,
    CONFIG_FINISHED,
    CONFIG_FAILED,
} StepperConfigState;

static volatile StepperConfigState configState = CONFIG_IDLE;
static uint8_t tiltDriverConfig[TILT_CONFIG_DATA_LEN];
static uint8_t panDriverConfig[PAN_CONFIG_DATA_LEN];
static volatile uint32_t configDeadlineTick = 0;

static bool config_deadline_reached(void)
{
    return (int32_t)(ControlTick10ms - configDeadlineTick) >= 0;
}

// Emm_V5(TILT) 与 X系列V2(PAN)字段布局不同；只映射语义相同的字段。
// PAN 独有字段（Lock/LPFilter/Vm_Limit/CurBW/S_PosTDP）保留自身当前值。
static void map_tilt_config_to_pan_v2(void)
{
    panDriverConfig[1] = (tiltDriverConfig[1] == 2u) ? 1u : 0u; // 闭环/开环控制模式
    panDriverConfig[2] = 1u;                                   // P_PUL：使能脉冲端口
    panDriverConfig[3] = tiltDriverConfig[2];                   // P_Serial
    panDriverConfig[4] = tiltDriverConfig[3];                   // En
    panDriverConfig[5] = tiltDriverConfig[4];                   // Dir
    panDriverConfig[6] = tiltDriverConfig[5];                   // MStep
    panDriverConfig[7] = tiltDriverConfig[6];                   // MPlyer
    panDriverConfig[8] = tiltDriverConfig[7];                   // AutoSDD
    panDriverConfig[10] = tiltDriverConfig[8];                  // Ma
    panDriverConfig[11] = tiltDriverConfig[9];
    panDriverConfig[12] = tiltDriverConfig[10];                 // Ma_Limit
    panDriverConfig[13] = tiltDriverConfig[11];
    panDriverConfig[18] = tiltDriverConfig[14];                 // UartBaud
    panDriverConfig[19] = tiltDriverConfig[15];                 // CAN_Baud
    panDriverConfig[20] = tiltDriverConfig[17];                 // Checksum（旧格式16为ID）
    panDriverConfig[21] = tiltDriverConfig[18];                 // Response
    panDriverConfig[23] = tiltDriverConfig[19];                 // Clog_Pro
    panDriverConfig[24] = tiltDriverConfig[20];                 // Clog_Rpm
    panDriverConfig[25] = tiltDriverConfig[21];
    panDriverConfig[26] = tiltDriverConfig[22];                 // Clog_Ma
    panDriverConfig[27] = tiltDriverConfig[23];
    panDriverConfig[28] = tiltDriverConfig[24];                 // Clog_Ms
    panDriverConfig[29] = tiltDriverConfig[25];
    panDriverConfig[30] = tiltDriverConfig[26];                 // 到位窗口
    panDriverConfig[31] = tiltDriverConfig[27];
}

// === 静态配置实例 ===
const StepperCfg panCfg = {
    .addr        = 0x01,
    .motType     = 1.8f,
    .mStep       = 16,
    .dirPolarity = 0,
    .vel         = 2000,
    .acc         = 200,
    .angleAbsMax =  180.0f,
    .angleAbsMin = -180.0f,
    .angleRelMax =  180.0f,    // pan 相对模式 ±180
    .angleRelMin = -180.0f,
};

const StepperCfg tiltCfg = {
    .addr        = 0x02,
    .motType     = 1.8f,
    .mStep       = 16,
    .dirPolarity = 0,
    .vel         = 2000,
    .acc         = 200,
    .angleAbsMax =  45.0f,
    .angleAbsMin = -45.0f,
    .angleRelMax =   45.0f,    // tilt 相对模式 ±45
    .angleRelMin =  -45.0f,
};

// 步进电机回零
void stepper_motor_reset_zero(void)
{
    Emm_V5_Origin_Trigger_Return(0x01, 0, false, UART_2_INST);
    Emm_V5_Origin_Trigger_Return(0x02, 0, false, UART_2_INST);
}


// 位置归零设置
void stepper_motor_set_zero(bool is_store)
{
    Emm_V5_Origin_Set_O(0x01, is_store, UART_2_INST);
    Emm_V5_Origin_Set_O(0x02, is_store, UART_2_INST);
}


// 控制步进电机的串口进行初始化
void stepper_motor_init(void)
{
    //串口初始化
    DL_UART_Main_clearInterruptStatus(UART_2_INST, DL_UART_MAIN_INTERRUPT_RX);
    DL_UART_Main_enableInterrupt(UART_2_INST, DL_UART_MAIN_INTERRUPT_RX);
    NVIC_EnableIRQ(UART_2_INST_INT_IRQN);

    //使用串口2并使能12多机同步
    Emm_V5_En_Control(1, true, false, UART_2_INST);
    Emm_V5_En_Control(2, true, false, UART_2_INST);

    stepper_motor_set_zero(true);  // 设置当前角度为零点并存储
    stepper_motor_reset_zero();    // 回零
}


// 请求一次性把 TILT(0x02) 的驱动配置复制并存储到 PAN(0x01)。
// 本函数可由按键中断调用，只置状态，不在中断中收发长帧。
void Stepper_RequestPanConfigCopy(void)
{
    configState = CONFIG_REQUESTED;
    stepper_config_status = 1;
}


// 主循环中的配置任务：读 TILT -> 写 PAN -> 收到成功回包后做 +10° 测试。
void Stepper_ConfigTask(void)
{
    uint8_t cmd[PAN_CONFIG_FRAME_LEN];

    switch (configState) {
        case CONFIG_REQUESTED:
            DL_UART_Main_clearInterruptStatus(UART_2_INST, DL_UART_MAIN_INTERRUPT_RX);
            DL_UART_Main_enableInterrupt(UART_2_INST, DL_UART_MAIN_INTERRUPT_RX);
            NVIC_ClearPendingIRQ(UART_2_INST_INT_IRQN);
            NVIC_EnableIRQ(UART_2_INST_INT_IRQN);

            configState = CONFIG_WAIT_TILT;
            configDeadlineTick = ControlTick10ms + CONFIG_REPLY_TIMEOUT_TICKS;
            Emm_V5_Read_Sys_Params(tiltCfg.addr, S_Conf, UART_2_INST);
            break;

        case CONFIG_WAIT_TILT:
            if (config_deadline_reached()) {
                configState = CONFIG_FAILED;
            }
            break;

        case CONFIG_READ_PAN_REQUESTED:
            configState = CONFIG_WAIT_PAN_SOURCE;
            configDeadlineTick = ControlTick10ms + CONFIG_REPLY_TIMEOUT_TICKS;
            Emm_V5_Read_Sys_Params(panCfg.addr, S_Conf, UART_2_INST);
            break;

        case CONFIG_WAIT_PAN_SOURCE:
            if (config_deadline_reached()) {
                configState = CONFIG_FAILED;
            }
            break;

        case CONFIG_READY_WRITE:
            cmd[0] = panCfg.addr;
            cmd[1] = 0x48;
            cmd[2] = 0xD1;
            cmd[3] = 0x01;  // 存入驱动器，后续上电不需要重复初始化
            for (uint8_t i = 0; i < PAN_CONFIG_DATA_LEN; ++i) {
                cmd[DRIVER_CONFIG_DATA_OFFSET + i] = panDriverConfig[i];
            }
            cmd[PAN_CONFIG_FRAME_LEN - 1u] = 0x6B;

            configState = CONFIG_WRITE_SETTLE;
            configDeadlineTick = ControlTick10ms + CONFIG_WRITE_SETTLE_TICKS;
            my_transmintData_s(UART_2_INST, cmd, PAN_CONFIG_FRAME_LEN);
            break;

        case CONFIG_WRITE_SETTLE:
            // 部分驱动器关闭了修改命令应答，超时后主动读回，不依赖 0x48 回包。
            if (config_deadline_reached()) {
                configState = CONFIG_VERIFY_REQUESTED;
            }
            break;

        case CONFIG_VERIFY_REQUESTED:
            configState = CONFIG_WAIT_PAN_VERIFY;
            configDeadlineTick = ControlTick10ms + CONFIG_REPLY_TIMEOUT_TICKS;
            Emm_V5_Read_Sys_Params(panCfg.addr, S_Conf, UART_2_INST);
            break;

        case CONFIG_WAIT_PAN_VERIFY:
            if (config_deadline_reached()) {
                configState = CONFIG_FAILED;
            }
            break;

        case CONFIG_TEST_READY:
            Emm_V5_En_Control(panCfg.addr, true, false, UART_2_INST);
            delay_ms(2);
            Emm_V5_En_Control(tiltCfg.addr, true, false, UART_2_INST);
            delay_ms(2);
            RelativeControl(10.0f, 10.0f);
            stepper_config_status = 2;
            configState = CONFIG_FINISHED;
            break;

        case CONFIG_FAILED:
            stepper_config_status = 3;
            configState = CONFIG_IDLE;
            break;

        default:
            break;
    }
}


// 脉冲计算
static StepperCmd calcAxis(float delta, const StepperCfg* cfg)
{
    float pulsesPerDeg = (float)cfg->mStep / cfg->motType;   // 16/0.9 ≈ 17.778
    float pulseF       = delta * pulsesPerDeg;
    int32_t pulseI     = (int32_t)(pulseF + (pulseF >= 0 ? 0.5f : -0.5f));

    uint8_t dir = (pulseI >= 0) ? 0 : 1;
    if (cfg->dirPolarity) dir = !dir;

    StepperCmd cmd;
    cmd.dir        = dir;
    cmd.clk        = (uint32_t)(pulseI >= 0 ? pulseI : -pulseI);
    cmd.angleDelta = delta;
    return cmd;
}


// ============================================================
//  控制层：软件设定点
//
//  MCU 自己维护"指令角度" cmd_deg[]，所有限位都在它身上做，再用绝对模式
//  开过去。回包(motorState)只用于监控、不参与限位——即使回包链路断了
//  (cnt 不涨)，限位依旧有效(fail-safe)。上电 stepper_motor_init() 已回零，
//  故 cmd_deg 初值 0 与硬件零点对齐。
// ============================================================

// 软件设定点 [0]=pan [1]=tilt（度）：当前指令位置
static float cmd_deg[2]    = {0.0f, 0.0f};
// 记录点：State_Record 拍下 cmd_deg 快照，State_Restore 据此归位
static float record_deg[2] = {0.0f, 0.0f};
static bool  record_valid  = false;   // 是否记录过有效姿态

// 限幅
static float clampf(float v, float min, float max)
{
    if (v > max) return max;
    if (v < min) return min;
    return v;
}

// 双轴发命令 + 多机同步。absolute=true 走绝对模式(raF=true)；false 走相对模式(raF=false)
static void sendStepper(StepperCmd pan, StepperCmd tilt, bool absolute)
{
    Emm_V5_Pos_Control(panCfg.addr, pan.dir, panCfg.vel, panCfg.acc,
                       pan.clk, absolute, true, UART_2_INST);
    delay_ms(2);
    Emm_V5_Pos_Control(tiltCfg.addr, tilt.dir, tiltCfg.vel, tiltCfg.acc,
                       tilt.clk, absolute, true, UART_2_INST);
    delay_ms(2);
    Emm_V5_Synchronous_motion(PRIMARY_BUS, UART_2_INST);
}

// 绝对控制器：把云台开到绝对角（按工作空间 ±limit 夹幅）。绝对模式(raF=true)脉冲以零点
// 为基准，故"目标角"本身即可经 calcAxis 转脉冲；末尾同步软件设定点。
void AbsoluteControl(float pan_target, float tilt_target)
{
    pan_target  = clampf(pan_target,  panCfg.angleAbsMin,  panCfg.angleAbsMax);
    tilt_target = clampf(tilt_target, tiltCfg.angleAbsMin, tiltCfg.angleAbsMax);

    sendStepper(calcAxis(pan_target, &panCfg), calcAxis(tilt_target, &tiltCfg), true);

    cmd_deg[0] = pan_target;
    cmd_deg[1] = tilt_target;
}

// 相对控制器：软件设定点累加增量 → 夹相对限位 → 发【相对模式(raF=false)】命令。
// 限位基准是本地 cmd_deg（不读回包，fail-safe）；相对模式让电机从当前物理位置挪
// real_delta，不依赖绝对帧，断电/重定零后也不会跳位。
void RelativeControl(float delta_pan, float delta_tilt)
{
    // 1. 目标 = 设定点 + 增量，夹相对限位
    float tgt_pan  = cmd_deg[0] + delta_pan;
    float tgt_tilt = clampf(cmd_deg[1] + delta_tilt, tiltCfg.angleRelMin, tiltCfg.angleRelMax);

#if PAN_RELATIVE_LIMIT_ENABLE
    tgt_pan = clampf(tgt_pan, panCfg.angleRelMin, panCfg.angleRelMax);
#endif

    // 2. 夹幅后实际可执行的增量
    float real_dp = tgt_pan  - cmd_deg[0];
    float real_dt = tgt_tilt - cmd_deg[1];

    // 3. 相对模式发命令（raF=false）
    sendStepper(calcAxis(real_dp, &panCfg), calcAxis(real_dt, &tiltCfg), false);

    // 4. 更新设定点（= 累计已发增量）
    cmd_deg[0] = tgt_pan;
    cmd_deg[1] = tgt_tilt;
}

// 记录当前姿态：拍下软件设定点快照
void State_Record(void)
{
    record_deg[0] = cmd_deg[0];
    record_deg[1] = cmd_deg[1];
    record_valid  = true;
}

// 恢复记录的姿态：开回记录点（没记录过就不动，避免误回 (0,0)）
void State_Restore(void)
{
    if (!record_valid) return;
    AbsoluteControl(record_deg[0], record_deg[1]);
}

// 只读访问（监控/打印用）
void Pose_GetCmd(float *pan, float *tilt)    { *pan = cmd_deg[0];    *tilt = cmd_deg[1];    }
void Pose_GetRecord(float *pan, float *tilt) { *pan = record_deg[0]; *tilt = record_deg[1]; }


// ============================================================
//  Emm_V5 回包解析
// ============================================================



// 协议解析状态机
typedef enum { RX_WAIT_ADDR, RX_WAIT_CMD, RX_COLLECT } RxState;
static struct {
    RxState  state;
    uint8_t  buf[PAN_CONFIG_FRAME_LEN];
    uint8_t  idx;
    uint8_t  expectLen;
} rxCtx = { RX_WAIT_ADDR, {0}, 0, 0 };

// 根据命令字返回完整帧长（含 addr + cmd + payload + 0x6B）
static uint8_t packet_expect_len(uint8_t addr, uint8_t cmd)
{
    switch (cmd) {
        case 0x31: return 5;   // S_ENCL: addr+cmd+H+L+0x6B
        case 0x33: return 8;   // S_TPOS: addr+cmd+sign+4B+0x6B
        case 0x35: return 6;   // S_VEL : addr+cmd+sign+H+L+0x6B
        case 0x36: return 8;   // S_CPOS
        case 0x37: return 8;   // S_PERR
        case 0x3A: return 4;   // S_FLAG: addr+cmd+flags+0x6B（暂未用）
        case 0x3B: return 4;   // S_ORG （暂未用）
        case 0x42: return (addr == panCfg.addr) ? PAN_CONFIG_FRAME_LEN
                                                : TILT_CONFIG_FRAME_LEN;
        case 0x48: return 4;   // 写驱动配置应答：addr+cmd+status+0x6B
        default:   return 0;   // 未知命令字，整包丢弃
    }
}

// 4 字节大端转无符号
static uint32_t bytes_be_u32(const volatile uint8_t* b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8)  | (uint32_t)b[3];
}

// Emm_V5 位置回包：65536 脉冲 = 360°
static float pulses_to_deg(uint32_t pulses, uint8_t sign)
{
    float deg = (float)pulses * 360.0f / 65536.0f;
    return sign ? -deg : deg;
}

// 解析一帧（已确认末字节是 0x6B），按 addr/cmd 分发到 motorState
static void dispatch_packet(const uint8_t* p, uint8_t len)
{
    uint8_t idx;
    if      (p[0] == panCfg.addr)  idx = 0;
    else if (p[0] == tiltCfg.addr) idx = 1;
    else return;

    switch (p[1]) {
        case 0x42: { // S_Conf：接收 TILT 源配置，或读回 PAN 做逐字节校验
            bool tiltFrameValid = len == TILT_CONFIG_FRAME_LEN &&
                                  p[2] == TILT_CONFIG_FRAME_LEN &&
                                  p[3] == TILT_CONFIG_PARAM_COUNT;
            bool panFrameValid = len == PAN_CONFIG_FRAME_LEN &&
                                 p[2] == PAN_CONFIG_FRAME_LEN &&
                                 p[3] == PAN_CONFIG_PARAM_COUNT;

            if (p[0] == tiltCfg.addr && configState == CONFIG_WAIT_TILT && tiltFrameValid) {
                for (uint8_t i = 0; i < TILT_CONFIG_DATA_LEN; ++i) {
                    tiltDriverConfig[i] = p[DRIVER_CONFIG_DATA_OFFSET + i];
                }
                configState = CONFIG_READ_PAN_REQUESTED;
            } else if (p[0] == panCfg.addr &&
                       configState == CONFIG_WAIT_PAN_SOURCE && panFrameValid) {
                for (uint8_t i = 0; i < PAN_CONFIG_DATA_LEN; ++i) {
                    panDriverConfig[i] = p[DRIVER_CONFIG_DATA_OFFSET + i];
                }
                map_tilt_config_to_pan_v2();
                configState = CONFIG_READY_WRITE;
            } else if (p[0] == panCfg.addr &&
                       configState == CONFIG_WAIT_PAN_VERIFY && panFrameValid) {
                bool same = true;
                for (uint8_t i = 0; i < PAN_CONFIG_DATA_LEN; ++i) {
                    // ID 地址必须保留 PAN 自己的 0x01，批量配置命令也不会修改该字段。
                    if (i != PAN_CONFIG_ID_OFFSET &&
                        p[DRIVER_CONFIG_DATA_OFFSET + i] != panDriverConfig[i]) {
                        same = false;
                        break;
                    }
                }
                configState = same ? CONFIG_TEST_READY : CONFIG_FAILED;
            } else if (configState == CONFIG_WAIT_TILT ||
                       configState == CONFIG_WAIT_PAN_SOURCE ||
                       configState == CONFIG_WAIT_PAN_VERIFY) {
                configState = CONFIG_FAILED;
            }
            break;
        }

        case 0x48:   // PAN 写配置回包，0x02 表示成功
            if (p[0] == panCfg.addr && configState == CONFIG_WRITE_SETTLE) {
                configState = (p[2] == 0x02) ? CONFIG_VERIFY_REQUESTED : CONFIG_FAILED;
            }
            break;

        case 0x31:   // S_ENCL: addr cmd H L 0x6B
            motorState[idx].encl = ((uint16_t)p[2] << 8) | p[3];
            break;

        case 0x33:   // S_TPOS: addr cmd sign b3 b2 b1 b0 0x6B
            motorState[idx].tpos_deg = pulses_to_deg(bytes_be_u32(&p[3]), p[2]);
            break;

        case 0x35: { // S_VEL: addr cmd sign H L 0x6B
            uint16_t rpm = ((uint16_t)p[3] << 8) | p[4];
            float    v   = (float)rpm;
            motorState[idx].vel_rpm = p[2] ? -v : v;
            break;
        }

        case 0x36:   // S_CPOS
            motorState[idx].cpos_deg = pulses_to_deg(bytes_be_u32(&p[3]), p[2]);
            break;

        case 0x37:   // S_PERR
            motorState[idx].perr_deg = pulses_to_deg(bytes_be_u32(&p[3]), p[2]);
            break;

        case 0x3A:   // S_FLAG: addr cmd flags 0x6B
            motorState[idx].flags = p[2];
            break;

        case 0x3B:   // S_ORG: addr cmd status 0x6B
            motorState[idx].org = p[2];
            break;

        default: break;
    }

    motorState[idx].updateCount++;
}

// 查询两轴实时位置（S_CPOS）；回包由 UART2_IRQHandler 异步写入 motorState（仅监控用）
void GetMotorState(void)
{
    Emm_V5_Read_Sys_Params(panCfg.addr,  S_CPOS, UART_2_INST);
    delay_ms(2);   // 等 PAN 回包收完，避免总线冲突
    Emm_V5_Read_Sys_Params(panCfg.addr,  S_FLAG, UART_2_INST);
    delay_ms(2);
    Emm_V5_Read_Sys_Params(tiltCfg.addr, S_CPOS, UART_2_INST);
    delay_ms(2);   // 等 TILT 回包收完
    Emm_V5_Read_Sys_Params(tiltCfg.addr, S_FLAG, UART_2_INST);
    delay_ms(2);
}

//解析电机回包
void UART2_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_2_INST))
    {
    case DL_UART_MAIN_IIDX_RX:
        while (!DL_UART_Main_isRXFIFOEmpty(UART_2_INST))
        {
            uint8_t b = DL_UART_Main_receiveData(UART_2_INST);

            switch (rxCtx.state) {
            case RX_WAIT_ADDR:
                // 仅以 0x01 / 0x02 作为帧起始（panCfg.addr / tiltCfg.addr）
                if (b == panCfg.addr || b == tiltCfg.addr) {
                    rxCtx.buf[0] = b;
                    rxCtx.idx = 1;
                    rxCtx.state = RX_WAIT_CMD;
                }
                break;

            case RX_WAIT_CMD:
                rxCtx.buf[rxCtx.idx++] = b;
                rxCtx.expectLen = packet_expect_len(rxCtx.buf[0], b);
                if (rxCtx.expectLen == 0) {
                    // 未知命令字，重置等下一个对齐点
                    rxCtx.state = RX_WAIT_ADDR;
                    rxCtx.idx   = 0;
                } else {
                    rxCtx.state = RX_COLLECT;
                }
                break;

            case RX_COLLECT:
                rxCtx.buf[rxCtx.idx++] = b;
                if (rxCtx.idx >= rxCtx.expectLen) {
                    // 末字节必须是 0x6B（Emm_V5 默认校验）
                    if (rxCtx.buf[rxCtx.idx - 1] == 0x6B) {
                        dispatch_packet((const uint8_t*)rxCtx.buf, rxCtx.idx);
                    }
                    rxCtx.state = RX_WAIT_ADDR;
                    rxCtx.idx   = 0;
                }
                break;
            }
        }
        break;
    default:
        break;
    }
}

// STEP_TIM (TIMG6)：10kHz / 0.1ms 周期中断，软件分频出周期标志。
// 注：定时器中断一旦使能就必须保留本处理函数、并在其中清中断标志（读 IIDX 即清），
//     否则缺失向量会落入 Default_Handler 死循环、整机卡死。
#define TICK_HZ           10000u
#define POSE_QUERY_TICKS  (TICK_HZ / 1000u * 50u)    // 50ms：触发一次位置查询
#define BIAS_PRINT_TICKS  (TICK_HZ / 1000u * 500u)   // 500ms：触发一次 bias 打印

void STEP_TIM_INST_IRQHandler(void)
{
    switch (DL_TimerG_getPendingInterrupt(STEP_TIM_INST)) {   // 读 IIDX 顺带清中断（TIMG6 是 TimerG）
    case DL_TIMER_IIDX_ZERO:
    {
        static uint16_t pos_cnt  = 0;
        static uint16_t bias_cnt = 0;
        if (++pos_cnt  >= POSE_QUERY_TICKS) { pos_cnt  = 0; pose_query_flag = 1; }
        if (++bias_cnt >= BIAS_PRINT_TICKS) { bias_cnt = 0; bias_print_flag = 1; }
        // lc_printf("tick=%lu\r\n", (unsigned long)pos_cnt);
        break;
    }
    default: break;
    }
}
