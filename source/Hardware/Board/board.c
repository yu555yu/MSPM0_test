


#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "Hardware/User/User.h"
#include "board.h"
#include "ti/driverlib/m0p/dl_core.h"


int fputc(int ch, FILE *f)
{
    (void)f;
    return ch;
}

/**
 * @brief 输出带文件/函数/行号信息的调试日志。
 *
 * 函数会格式化日志前缀，附加用户提供的消息，然后通过 UART0 发送。
 *
 * @param __file 源文件名，通常传入 __FILE__。
 * @param __func 函数名，通常传入 __func__。
 * @param __line 源代码行号，通常传入 __LINE__。
 * @param format printf 风格格式字符串。
 * @param ... 其它格式参数。
 * @return vsnprintf 返回的写入字符数。
 */
int LOG_Debug_Out(const char* __file, const char* __func, int __line, const char* format, ...)
{
    (void)__file; (void)__func; (void)__line; (void)format;
    return 0;
}

/**
 * @brief 通过 UART0 打印格式化字符串。
 *
 * 该函数类似 printf，但不会附加文件/函数/行号信息，直接发送到调试串口。
 *
 * @param format printf 风格格式字符串。
 * @param ... 其它格式参数。
 * @return vsnprintf 返回的写入字符数。
 */
int lc_printf(char* format,...)
{
    (void)format;
    return 0;
}


/* ================ 延时函数封装 =================== */

/**
 * @brief 延时指定的微秒数。
 *
 * 通过 CPU 时钟循环计数实现忙等待延时，遇到中断时会延长实际时间。
 */
void delay_us(int __us) { delay_cycles((CPUCLK_FREQ / 1000 / 1000) * __us); }

/**
 * @brief 延时指定的毫秒数。
 */
void delay_ms(int __ms) { delay_cycles((CPUCLK_FREQ / 1000) * __ms); }

/**
 * @brief delay_us 的别名。
 *
 * 该函数用于兼容使用 _1us/_1ms 命名风格的代码。
 */
void delay_1us(int __us) { delay_cycles((CPUCLK_FREQ / 1000 / 1000) * __us); }

/**
 * @brief delay_ms 的别名。
 */
void delay_1ms(int __ms) { delay_cycles((CPUCLK_FREQ / 1000) * __ms); }


/* ================ UART1 接口函数 =================== */

/** @brief UART1 接收缓冲区 */
volatile uint8_t uart1_rx_buffer[256];
/** @brief UART1 接收数据长度 */
volatile uint16_t uart1_rx_len = 0;

/**
 * @brief 初始化 UART1 并使能接收中断。
 *
 * 在 SysConfig 已完成硬件配置的基础上，额外使能接收 FIFO 中断
 * 并打开 NVIC 中断。
 */
void uart1_init(void)
{
    /* 清空 RX FIFO 中所有残留数据 */
    while (DL_UART_Main_isRXFIFOEmpty(UART_1_INST) == false) {
        DL_UART_Main_receiveData(UART_1_INST);
    }
    DL_UART_Main_clearInterruptStatus(UART_1_INST, DL_UART_MAIN_INTERRUPT_RX);
    DL_UART_Main_enableInterrupt(UART_1_INST, DL_UART_MAIN_INTERRUPT_RX);
    NVIC_EnableIRQ(UART_1_INST_INT_IRQN);
}

/**
 * @brief 通过 UART1 阻塞发送一个字节。
 *
 * @param dat 要发送的字节。
 */
void uart1_sendChar(uint8_t dat)
{
    while (DL_UART_isBusy(UART_1_INST) == true);
    DL_UART_Main_transmitData(UART_1_INST, dat);
}

/**
 * @brief 通过 UART1 发送以 \0 结尾的字符串。
 *
 * @param str 要发送的字符串。
 */
void uart1_sendString(char* str)
{
    while (str != NULL && *str != 0)
    {
        uart1_sendChar((uint8_t)*str++);
    }
}

/* ── 解析结果 ── */
volatile uint16_t uart1_rx_Cmd   = 0;   // 0=IDLE 1=AIM 2=FIRE
volatile uint16_t uart1_rx_Task  = 0;   // 任务号 0~6
volatile int16_t  uart1_rx_BiasX = 0;
volatile int16_t  uart1_rx_BiasY = 0;
//二维云台数据标志
volatile uint8_t uart1_rx_flag = 0;
/* ── 原始数据包（调试用）── */
volatile uint8_t uart1_rx_raw[9] = {0};   // 最近一次收到的原始 8 数据字节 + 1 校验和
volatile uint8_t uart1_rx_raw_flag = 0;   // 新原始数据包标志

/* ── UART1 ISR 诊断计数器（LED 诊断用） ── */
volatile uint16_t uart1_diag_sync     = 0;  // 收到 0xFF 的次数
volatile uint16_t uart1_diag_frame_ok = 0;  // 完整帧解析成功次数
volatile uint16_t uart1_diag_st2_fail = 0;  // checksum 失败次数

volatile uint32_t Tick = 0;


/**
 * @brief 获取 UART1 解析后的数据。
 *
 * 该函数将解析后的 UART1 接收数据赋值给传入的指针参数。
 * 数据包括命令、任务号以及 X 和 Y 的偏置值。
 *
 * @param cmd 指向存储命令值的指针。
 * @param task 指向存储任务号的指针。
 * @param bias_x 指向存储 X 偏置值的指针。
 * @param bias_y 指向存储 Y 偏置值的指针。
 */
void UART1_GetParsedData(uint16_t *cmd, uint16_t *task, int16_t *bias_x, int16_t *bias_y)
{
    *cmd    = uart1_rx_Cmd;
    *task   = uart1_rx_Task;
    *bias_x = uart1_rx_BiasX;
    *bias_y = uart1_rx_BiasY;
}

void SysTick_Handler(void) {
    Tick_SysTickCallback();
}
void Tick_delay(uint32_t t) {
    uint32_t tEnd = Tick + t;
    while (Tick < tEnd);
}
// SysTick中断回调(1ms)
void Tick_SysTickCallback(void) {
    Tick++;
}
/**
 * @brief UART1 中断处理函数。
 *
 * 协议：0xFF + [8字节数据] + [1字节校验和]
 * 收满 9 字节立即校验解析，不要求任何帧尾。
 * 数据包包含 Cmd、Task、BiasX、BiasY 四个 16位值。
 */
void UART1_IRQHandler(void)
{
    static uint8_t  rx_state = 0;
    static uint8_t  rx_packet[9];
    static uint8_t  rx_index = 0;

    switch (DL_UART_Main_getPendingInterrupt(UART_1_INST))
    {
    case DL_UART_MAIN_IIDX_RX:
        while (DL_UART_Main_isRXFIFOEmpty(UART_1_INST) == false)
        {
            uint8_t dat = DL_UART_Main_receiveData(UART_1_INST);

            switch (rx_state)
            {
            case 0:   /* 等帧头 0xFF */
                if (dat == 0xFF) { rx_index = 0; rx_state = 1; uart1_diag_sync++; }
                break;
            case 1:   /* 收 9 字节（8数据+1校验），收满即解 */
                rx_packet[rx_index++] = dat;
                if (rx_index >= 9)
                {
                    uint8_t sum = 0;
                    for (uint8_t i = 0; i < 8; i++) sum += rx_packet[i];
                    if (sum == rx_packet[8])
                    {
                        uart1_rx_Cmd   = ((uint16_t)rx_packet[0] << 8) | rx_packet[1];
                        uart1_rx_Task  = ((uint16_t)rx_packet[2] << 8) | rx_packet[3];
                        uart1_rx_BiasX = (int16_t)(((uint16_t)rx_packet[4] << 8 | rx_packet[5]) - 32768u);
                        uart1_rx_BiasY = (int16_t)(((uint16_t)rx_packet[6] << 8 | rx_packet[7]) - 32768u);
                        uart1_rx_flag  = 1;
                        for (uint8_t i = 0; i < 9; i++) uart1_rx_raw[i] = rx_packet[i];
                        uart1_rx_raw_flag = 1;
                        uart1_diag_frame_ok++;
                    }
                    else
                    {
                        uart1_diag_st2_fail++;
                    }
                    rx_state = 0;
                }
                break;
            default:
                rx_state = 0;
                break;
            }
        }
        break;
    default:
        break;
    }
}

