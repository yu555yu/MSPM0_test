/**
 * @file serial_ctrl.h
 * @brief Jetson视觉串口通信协议解析模块
 * @note 12字节帧格式：0xA5 0x5A + task_id + vision_state + pos(2) + vel(2) + target(2) + seq + crc8
 */

#ifndef SERIAL_CTRL_H
#define SERIAL_CTRL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 协议常量 ========== */
#define VISION_FRAME_HEADER1    0xA5
#define VISION_FRAME_HEADER2    0x5A
#define VISION_FRAME_SIZE       12

/* ========== 视觉状态枚举 ========== */
typedef enum {
    VISION_STATE_LOST = 0,      /* 丢失目标 */
    VISION_STATE_TRACK = 1,     /* 跟踪中 */
    VISION_STATE_PREDICT = 2    /* 预测模式 */
} VisionState_t;

/* ========== 任务ID枚举 ========== */
typedef enum {
    VISION_TASK_IDLE = 0,       /* 空闲 */
    VISION_TASK_3 = 3,          /* 题目第3项 */
    VISION_TASK_4 = 4,          /* 题目第4项 */
    VISION_TASK_5 = 5,          /* 题目第5项 */
    VISION_TASK_6 = 6           /* 题目第6项 */
} VisionTask_t;

/* ========== 数据快照结构 ========== */
typedef struct {
    /* 原始数据 */
    uint8_t  task_id;           /* 任务ID */
    uint8_t  vision_state;      /* 视觉状态 */
    int16_t  position_raw;      /* 原始位置（int16） */
    int16_t  velocity_raw;      /* 原始速度（int16） */
    int16_t  target_raw;        /* 原始目标（int16） */

    /* 物理量转换（单位：cm） */
    float    position_cm;       /* 位置（cm），中心为0，左负右正 */
    float    velocity_cm_s;     /* 速度（cm/s），左负右正 */
    float    target_cm;         /* 目标位置（cm） */

    /* 序列号 */
    uint8_t  task_seq;          /* 任务切换序列号 */
    uint8_t  frame_seq;         /* 帧序号 */

    /* 统计信息 */
    uint32_t valid_frame_count;    /* 有效帧计数 */
    uint32_t crc_error_count;      /* CRC错误计数 */
    uint32_t timeout_count;        /* 超时计数 */

    /* 状态标志 */
    uint8_t  task_changed;      /* 任务ID变化标志（0=未变，1=已变） */
    uint8_t  vision_fresh;      /* 数据新鲜度标志（0=旧数据，1=新数据） */
} Serial_Data;

/* ========== 公共接口 ========== */

/**
 * @brief 初始化视觉串口通信模块
 */
void Serial_Init(void);

/**
 * @brief UART接收字节中断服务函数
 * @param byte 接收到的单字节数据
 * @note 必须在UART1_IRQHandler中调用
 */
void Serial_RxByteISR(uint8_t byte);

/**
 * @brief 主循环任务函数
 * @note 在main()主循环中调用，处理数据更新和状态维护
 */
void Serial_Task(void);

/**
 * @brief 10ms定时任务
 * @note 在TIMER_0中断中调用，用于超时检测
 */
void Serial_Tick10ms(void);

/**
 * @brief 获取数据快照
 * @param data 指向数据结构的指针
 * @return true=成功获取有效数据，false=无有效数据
 */
bool Serial_GetData(Serial_Data *data);

/**
 * @brief 原子获取并消费一帧新的有效数据
 * @param data 指向接收快照的结构体
 * @return true=本次获取到未处理的新帧，false=无新帧或通信已超时
 * @note 用于TIMER_0中断，避免复制过程被UART1中断改写
 */
bool Serial_TakeFreshData(Serial_Data *data);

/**
 * @brief 检查数据是否新鲜
 * @return true=数据新鲜，false=数据陈旧
 */
bool Serial_IsFresh(void);

/**
 * @brief 清除数据新鲜标志
 * @note 读取数据后调用，避免重复处理同一帧数据
 */
void Serial_ClearFresh(void);

/* ========== 调试接口 ========== */

/**
 * @brief 获取诊断信息
 * @param sync_count 帧头同步计数
 * @param frame_ok 有效帧计数
 * @param crc_err CRC错误计数
 * @param timeout 超时计数
 */
void Serial_GetDiagnostics(uint32_t *sync_count, uint32_t *frame_ok,
                            uint32_t *crc_err, uint32_t *timeout);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_CTRL_H */
