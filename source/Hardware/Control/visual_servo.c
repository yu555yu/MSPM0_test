#include <stdint.h>
#include "Hardware/Board/board.h"
#include "Hardware/Motor/stepper_motor.h"
#include "Hardware/Buzzer&Light/B&L.h"
#include "Hardware/Control/visual_servo.h"

// ============================================================
//  视觉伺服控制律（第二题：上电标定准心 → 转到靶心 → 开火）
//
//  数据流：Jetson(main.py) 算 bias=准心−靶心 → serial2MCU 发 12 字节帧
//          → board.c UART1 解包出 Cmd/Bias → 本模块比例驱动云台+激光。
//
//  相机装在云台上：激光落点≈固定像素=准心。云台朝"减小 bias"方向转，
//  靶心就被怼到准心，激光即命中。纯比例反馈，不依赖绝对位姿。
// ============================================================


// 像素偏差→角速度，再乘实际控制周期得到本帧角增量，避免响应随视觉帧率变化。
// 符号决定旋转方向：上电后若云台朝"反方向"跑飞，把对应增益改成相反符号
#define KP_NEAR             (+0.30f)   // 小偏差角速度增益 (度/秒/像素)
#define KP_MID              (+0.55f)   // 中等偏差角速度增益 (度/秒/像素)
#define KP_FAR              (+0.80f)   // 大偏差角速度增益 (度/秒/像素)
#define AIM_BIAS_NEAR_PX     20
#define AIM_BIAS_FAR_PX      90
#define PAN_RATE_MAX_DPS      90.0f
#define TILT_RATE_MAX_DPS     60.0f
#define YAW_FF_GAIN          (+0.85f)  // 车体转动时云台反向前馈，符号错误时只改这里
#define CONTROL_TICK_S        0.010f
#define AIM_DT_MAX_S          0.030f
#define YAW_FF_DEADBAND_DPS   3.0f

#define AIM_DEADBAND_PX      2          // |bias|≤此值 视为已对准，不再微调(防抖)
#define AIM_STEP_MAX_DEG      5.0f      // 高频跟随单步限幅，防止旧帧连续累加导致过冲
#define AIM_SETTLE_MS        150        // 仅 Task3：发一步后等云台走完，再吃下一帧 bias

// —— Task3 旋转寻靶（纯 pan 横扫，开环来回弹）——
#define SEARCH_PAN_MAX_DEG  80.0f       // pan 绝对扫描幅度 ±此值，到边反向
#define SEARCH_STEP_DEG      27.0f       // 每帧 SEARCH 横扫步进 (度) — 小步匀速
#define SEARCH_SETTLE_MS      350        // 发一步后等云台走完 + 视觉看清再继续扫

// ===================== 串口命令字（与 serial2MCU.py 对齐）=====
#define CMD_IDLE             0          // 停车，激光灭
#define CMD_AIM              1          // 按 bias 修偏，激光灭（还在瞄）
#define CMD_FIRE             2          // 按 bias 修偏，激光常亮（已瞄准/标定）
#define CMD_RECALL           3          // 回记录点（Task3 用，本轮不处理）
#define CMD_SEARCH           4          // 旋转寻靶：AbsoluteControl 横扫 pan 找靶

// 单步角增量限幅：防一帧大 bias 把云台一次甩太远
static float clamp_step(float d)
{
    if (d >  AIM_STEP_MAX_DEG) return  AIM_STEP_MAX_DEG;
    if (d < -AIM_STEP_MAX_DEG) return -AIM_STEP_MAX_DEG;
    return d;
}

static float clamp_rate(float rate, float max_abs)
{
    if (rate >  max_abs) return  max_abs;
    if (rate < -max_abs) return -max_abs;
    return rate;
}

static float gain_for_bias(int16_t abs_bias);

// Task3 旋转寻靶：从当前设定点续扫，pan 在 ±SEARCH_PAN_MAX_DEG 间来回弹。
// 用 AbsoluteControl(绝对角)而非 RelativeControl(累加)——cmd_deg 被直接设成扫描
// 绝对角，永不贴 ±180 限位，找到靶交给 AIM 时仍有满量程余量，绝不自锁。
static int8_t search_dir = +1;          // +1 右扫 / -1 左扫
static void Aim_Search(void)
{
    float cur_pan, cur_tilt;
    Pose_GetCmd(&cur_pan, &cur_tilt);   // 从当前设定点续扫，不跳回

    float next = cur_pan + (float)search_dir * SEARCH_STEP_DEG;
    if (next >=  SEARCH_PAN_MAX_DEG) { next =  SEARCH_PAN_MAX_DEG; search_dir = -1; }
    if (next <= -SEARCH_PAN_MAX_DEG) { next = -SEARCH_PAN_MAX_DEG; search_dir = +1; }

    AbsoluteControl(next, cur_tilt);    // tilt 保持不动，只横扫 pan
    delay_ms(SEARCH_SETTLE_MS);
}

// 主循环每圈调用：仅当上位机来了新一帧(uart1_rx_flag)才动作
void Aim_Update(void)
{
    if (uart1_rx_flag == 0) return;            // 没有新数据，直接返回

    uint16_t cmd, task;
    int16_t  bx, by;
    UART1_GetParsedData(&cmd, &task, &bx, &by);
    uart1_rx_flag = 0;

    extern volatile float YawRate;
    extern volatile uint32_t ControlTick10ms;
    static uint32_t last_control_tick = 0;
    uint32_t now_tick = ControlTick10ms;
    uint32_t elapsed_ticks = now_tick - last_control_tick;
    last_control_tick = now_tick;
    float control_dt = (float)elapsed_ticks * CONTROL_TICK_S;
    if (control_dt < CONTROL_TICK_S) control_dt = CONTROL_TICK_S;
    if (control_dt > AIM_DT_MAX_S) control_dt = AIM_DT_MAX_S;

    // —— 激光：FIRE 常亮，其余熄灭（Light_ON/OFF 幂等，不会像 Toggle 那样闪）——
    if (cmd == CMD_FIRE) Light_ON();
    else                 Light_OFF();

    // —— Task3 寻靶：没看到靶就横扫 pan 找（此时激光是灭的）——
    if (cmd == CMD_SEARCH) { Aim_Search(); return; }

    // —— 云台：IDLE 不动；只有 AIM/FIRE 才比例修偏 ——
    if (cmd != CMD_AIM && cmd != CMD_FIRE) return;

    // 死区：已对准就不再抖动。标定时 Jetson 发 FIRE 0,0 也走这里→只亮激光不动
    int16_t ax = (bx >= 0) ? bx : -bx;
    int16_t ay = (by >= 0) ? by : -by;
    if (ax <= AIM_DEADBAND_PX) bx = 0;
    if (ay <= AIM_DEADBAND_PX) by = 0;

    float yaw_abs = (YawRate >= 0.0f) ? YawRate : -YawRate;
    if (bx == 0 && by == 0 &&
        ((task != 4 && task != 5) || yaw_abs <= YAW_FF_DEADBAND_DPS)) return;

    float pan_rate_dps  = gain_for_bias(ax) * (float)bx;
    float tilt_rate_dps = gain_for_bias(ay) * (float)by;

    if (task == 4 || task == 5)
    {
        pan_rate_dps -= YAW_FF_GAIN * YawRate;
    }

    pan_rate_dps  = clamp_rate(pan_rate_dps,  PAN_RATE_MAX_DPS);
    tilt_rate_dps = clamp_rate(tilt_rate_dps, TILT_RATE_MAX_DPS);

    float dpan  = clamp_step(pan_rate_dps  * control_dt);
    float dtilt = clamp_step(tilt_rate_dps * control_dt);

    RelativeControl(dpan, dtilt);              // 在软件设定点上相对 jog（带 ±limit 硬保护）
    if (task == 3)
    {
        delay_ms(AIM_SETTLE_MS);               // Task3 搜索/停稳打靶才需要等待
    }
}

static float gain_for_bias(int16_t abs_bias)
{
    if (abs_bias >= AIM_BIAS_FAR_PX) return KP_FAR;
    if (abs_bias >= AIM_BIAS_NEAR_PX) return KP_MID;
    return KP_NEAR;
}
