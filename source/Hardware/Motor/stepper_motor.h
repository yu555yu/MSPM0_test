#ifndef __STEPPER_MOTOR_H
#define __STEPPER_MOTOR_H

#include <stdint.h>
#include <stdbool.h>

//总线定义
#define PRIMARY_BUS  0

extern volatile uint8_t pose_query_flag;
extern volatile uint8_t bias_print_flag;
// TILT -> PAN 一次性驱动参数复制状态：0空闲 / 1进行中 / 2成功 / 3失败
extern volatile uint8_t stepper_config_status;


typedef struct {
    uint8_t   addr;
    float     motType;
    uint16_t  mStep;
    uint8_t   dirPolarity;
    uint16_t  vel;
    uint8_t   acc;
    // 绝对模式限位（AbsoluteControl 用：工作空间上限）
    float     angleAbsMax;
    float     angleAbsMin;
    // 相对模式限位（RelativeControl 用：视觉跟踪环路硬保护，防 CSI 线扯坏）
    float     angleRelMax;
    float     angleRelMin;
} StepperCfg;

typedef struct {
    uint8_t   dir;
    uint32_t  clk;
    float     angleDelta;

} StepperCmd;

// 电机回包状态（由 UART2_IRQHandler 异步更新；按 addr 索引：[0]=pan, [1]=tilt）
typedef struct {
    float    cpos_deg;       // S_CPOS 实时位置（度）
    float    tpos_deg;       // S_TPOS 目标位置（度）
    float    perr_deg;       // S_PERR 位置误差（度）
    float    vel_rpm;        // S_VEL  实时速度（RPM）
    uint16_t encl;           // S_ENCL 编码器线性化值
    uint8_t  flags;          // S_FLAG：bit0=使能, bit1=到位, bit2=堵转
    uint8_t  org;            // S_ORG： 回零状态标志位
    uint32_t updateCount;    // 收包计数（主循环可监控是否在涨）
} MotorState;

extern volatile MotorState motorState[2];

extern const StepperCfg panCfg;
extern const StepperCfg tiltCfg;

void stepper_motor_init(void);
void stepper_motor_reset_zero(void);
void stepper_motor_set_zero(bool is_store);
void GetMotorState(void);
void Stepper_RequestPanConfigCopy(void);
void Stepper_ConfigTask(void);


// === 控制器 ===
void AbsoluteControl(float pan_target, float tilt_target);  // 开到绝对角（工作空间 ±limit）
void RelativeControl(float delta_pan,  float delta_tilt);   // 在软件设定点上 jog（相对 ±limit）

// === 姿态记录/恢复 ===
void State_Record(void);
void State_Restore(void);

// === 只读访问（监控/打印用）===
void Pose_GetCmd(float *pan, float *tilt);     // 当前软件设定点
void Pose_GetRecord(float *pan, float *tilt);  // 已记录的设定点

#endif
