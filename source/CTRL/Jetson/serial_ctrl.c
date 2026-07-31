/**
 * @file serial_ctrl.c
 * @brief Jetson视觉串口通信协议解析模块实现
 */

#include "serial_ctrl.h"
#include "ti_msp_dl_config.h"
#include <string.h>

/* ========== 接收状态机 ========== */
typedef enum {
    RX_STATE_IDLE = 0,       /* 等待帧头1 (0xA5) */
    RX_STATE_HEADER2,        /* 等待帧头2 (0x5A) */
    RX_STATE_DATA,           /* 接收数据字节 */
    RX_STATE_CRC8            /* 接收CRC8 */
} RxState_t;

/* ========== 内部状态 ========== */
static struct {
    /* 接收状态机 */
    RxState_t      rx_state;
    uint8_t        rx_buffer[VISION_FRAME_SIZE];
    uint8_t        rx_index;

    /* 当前有效数据 */
    Serial_Data current_data;

    /* 诊断计数器 */
    uint32_t       sync_count;        /* 帧头同步次数 */
    uint32_t       timeout_counter;   /* 超时计数 */

    /* 超时检测 */
    uint16_t       timeout_tick;     /* 10ms tick计数 */
    uint8_t        timeout_flag;     /* 超时标志 */

    /* 上一帧任务ID（用于检测任务切换） */
    uint8_t        last_task_id;
} g_serial_link = {
    .rx_state = RX_STATE_IDLE,
    .rx_index = 0,
    .timeout_tick = 0,
    .timeout_flag = 0,
    .last_task_id = 0xFF
};

/* ========== CRC8计算 ========== */
/**
 * @brief 计算CRC8校验码
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return CRC8校验值
 * @note 多项式：0x07，初始值：0x00，输入反转：是，输出反转：是
 */
static uint8_t CRC8_Calculate(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00;
    uint8_t i, bit;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/* ========== 大端字节序转换 ========== */
/**
 * @brief 大端字节序转int16
 */
static int16_t BE_ToInt16(const uint8_t *bytes)
{
    return (int16_t)((bytes[0] << 8) | bytes[1]);
}

/* ========== 公共接口实现 ========== */

void Serial_Init(void)
{
    memset(&g_serial_link, 0, sizeof(g_serial_link));
    g_serial_link.rx_state = RX_STATE_IDLE;
    g_serial_link.last_task_id = 0xFF;

    /* 清空上电残留字节，并显式开启UART1 RX中断和NVIC通道。 */
    while (!DL_UART_Main_isRXFIFOEmpty(UART_1_INST)) {
        (void)DL_UART_Main_receiveData(UART_1_INST);
    }
    DL_UART_Main_clearInterruptStatus(UART_1_INST, DL_UART_MAIN_INTERRUPT_RX);
    DL_UART_Main_enableInterrupt(UART_1_INST, DL_UART_MAIN_INTERRUPT_RX);
    NVIC_ClearPendingIRQ(UART_1_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_1_INST_INT_IRQN);
}

void Serial_RxByteISR(uint8_t byte)
{
    switch (g_serial_link.rx_state) {
    case RX_STATE_IDLE:
        if (byte == VISION_FRAME_HEADER1) {
            g_serial_link.rx_state = RX_STATE_HEADER2;
            g_serial_link.rx_index = 0;
            g_serial_link.rx_buffer[0] = byte;
        }
        break;

    case RX_STATE_HEADER2:
        if (byte == VISION_FRAME_HEADER2) {
            g_serial_link.rx_buffer[1] = byte;
            g_serial_link.rx_state = RX_STATE_DATA;
            g_serial_link.rx_index = 2;  /* 从第2字节开始存储 */
        } else {
            g_serial_link.rx_state = RX_STATE_IDLE;
        }
        break;

    case RX_STATE_DATA:
        g_serial_link.rx_buffer[g_serial_link.rx_index++] = byte;
        if (g_serial_link.rx_index >= 11) {  /* 已接收字节2~10 */
            g_serial_link.rx_state = RX_STATE_CRC8;
        }
        break;

    case RX_STATE_CRC8:
        g_serial_link.rx_buffer[11] = byte;

        /* CRC8校验：对字节2~10计算 */
        uint8_t calculated_crc = CRC8_Calculate(&g_serial_link.rx_buffer[2], 9);
        if (calculated_crc == byte) {
            /* CRC校验通过，解析数据 */
            g_serial_link.sync_count++;

            /* 解析基本字段 */
            g_serial_link.current_data.task_id = g_serial_link.rx_buffer[2];
            g_serial_link.current_data.vision_state = g_serial_link.rx_buffer[3];
            g_serial_link.current_data.position_raw = BE_ToInt16(&g_serial_link.rx_buffer[4]);
            g_serial_link.current_data.velocity_raw = BE_ToInt16(&g_serial_link.rx_buffer[6]);
            g_serial_link.current_data.target_raw = BE_ToInt16(&g_serial_link.rx_buffer[8]);
            g_serial_link.current_data.frame_seq = g_serial_link.rx_buffer[10];

            /* 物理量转换（0.01cm → cm） */
            g_serial_link.current_data.position_cm = g_serial_link.current_data.position_raw * 0.01f;
            g_serial_link.current_data.velocity_cm_s = g_serial_link.current_data.velocity_raw * 0.01f;
            g_serial_link.current_data.target_cm = g_serial_link.current_data.target_raw * 0.01f;

            /* 检测任务切换 */
            if (g_serial_link.last_task_id != g_serial_link.current_data.task_id) {
                if (g_serial_link.last_task_id != 0xFF) {  /* 非首次初始化 */
                    g_serial_link.current_data.task_changed = 1;
                    g_serial_link.current_data.task_seq++;
                }
                g_serial_link.last_task_id = g_serial_link.current_data.task_id;
            }

            /* 更新统计 */
            g_serial_link.current_data.valid_frame_count++;
            g_serial_link.current_data.vision_fresh = 1;

            /* 重置超时计数 */
            g_serial_link.timeout_tick = 0;
            g_serial_link.timeout_flag = 0;
        } else {
            /* CRC校验失败 */
            g_serial_link.current_data.crc_error_count++;
        }

        /* 重置状态机 */
        g_serial_link.rx_state = RX_STATE_IDLE;
        g_serial_link.rx_index = 0;
        break;

    default:
        g_serial_link.rx_state = RX_STATE_IDLE;
        break;
    }
}

void Serial_Task(void)
{
    /* 主循环任务：可在此处添加数据处理逻辑 */
    /* 当前实现：数据在ISR中直接更新到current_data */
}

void Serial_Tick10ms(void)
{
    /* 超时检测：500ms无数据则认为超时 */
    if (g_serial_link.timeout_tick < 50) {
        g_serial_link.timeout_tick++;
    } else if (!g_serial_link.timeout_flag) {
        g_serial_link.timeout_flag = 1;
        g_serial_link.current_data.timeout_count++;
        g_serial_link.current_data.vision_fresh = 0;
    }
}

bool Serial_GetData(Serial_Data *data)
{
    if (data == NULL) {
        return false;
    }

    /* 复制当前数据 */
    memcpy(data, &g_serial_link.current_data, sizeof(Serial_Data));

    /* 有有效数据且未超时 */
    return (g_serial_link.current_data.valid_frame_count > 0) &&
           (!g_serial_link.timeout_flag);
}

bool Serial_TakeFreshData(Serial_Data *data)
{
    bool has_fresh_data;
    uint32_t primask;

    if (data == NULL) {
        return false;
    }

    /* 快照与清Fresh必须在同一临界区完成，否则可能混入下一帧字段。 */
    primask = __get_PRIMASK();
    __disable_irq();

    has_fresh_data =
        (g_serial_link.current_data.vision_fresh != 0U) &&
        (g_serial_link.current_data.valid_frame_count > 0U) &&
        (g_serial_link.timeout_flag == 0U);

    if (has_fresh_data) {
        memcpy(data, &g_serial_link.current_data, sizeof(Serial_Data));
        g_serial_link.current_data.vision_fresh = 0U;
    }

    if (primask == 0U) {
        __enable_irq();
    }

    return has_fresh_data;
}

bool Serial_IsFresh(void)
{
    return g_serial_link.current_data.vision_fresh;
}

void Serial_ClearFresh(void)
{
    g_serial_link.current_data.vision_fresh = 0;
}

void Serial_GetDiagnostics(uint32_t *sync_count, uint32_t *frame_ok,
                            uint32_t *crc_err, uint32_t *timeout)
{
    if (sync_count) *sync_count = g_serial_link.sync_count;
    if (frame_ok)   *frame_ok = g_serial_link.current_data.valid_frame_count;
    if (crc_err)    *crc_err = g_serial_link.current_data.crc_error_count;
    if (timeout)   *timeout = g_serial_link.current_data.timeout_count;
}
