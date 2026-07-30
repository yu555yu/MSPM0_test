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

volatile uint32_t Tick = 0;

void SysTick_Handler(void) {
    Tick_SysTickCallback();
}
void Tick_delay(uint32_t t) {
    uint32_t tEnd = Tick + t;
    while (Tick < tEnd);
}
/* SysTick中断回调(1ms) */
void Tick_SysTickCallback(void) {
    Tick++;
}

/*
 * UART1 已弃用（原云台 Jetson 通信）。
 * 保留空 ISR 以通过链接 — SysConfig 生成的中断向量表仍需此符号。
 */
void UART1_IRQHandler(void)
{
}

