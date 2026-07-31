#include "mid_laptime.h"
#include "Hardware/LCD/lcd.h"
#include "ti_msp_dl_config.h"

/* ---- LCD 显示位置 --------------------------------------------------------- */

#define LAP_TIME_DISPLAY_X  58U
#define LAP_TIME_DISPLAY_Y  70U

/* ---- 内部标志 ------------------------------------------------------------- */

#define LAP_TIME_DISPLAY_RUNNING   1U
#define LAP_TIME_DISPLAY_FINISHED  2U

static volatile uint8_t display_request = 0U;
static volatile uint8_t finish_line_ignore = 0U;
static volatile uint8_t was_running = 0U;
static volatile uint8_t display_tick = 0U;

/* ---- 全局状态 (对外暴露) --------------------------------------------------- */

volatile uint32_t LapTime10ms = 0U;
volatile uint8_t  lap_running = 0U;

/* ---- 格式化 --------------------------------------------------------------- */

static void format_time(char *text, uint32_t time_10ms)
{
    uint32_t seconds;
    uint32_t hundredths;
    uint32_t divisor;
    uint8_t index = 0U;
    uint8_t digit_started = 0U;
    const char prefix[] = "TIME:";

    if (time_10ms > 9999999U)
    {
        time_10ms = 9999999U;
    }

    seconds    = time_10ms / 100U;
    hundredths = time_10ms % 100U;

    while (prefix[index] != '\0')
    {
        text[index] = prefix[index];
        index++;
    }

    for (divisor = 10000U; divisor > 0U; divisor /= 10U)
    {
        uint8_t digit = (uint8_t)(seconds / divisor);
        seconds %= divisor;

        if ((digit == 0U) && (digit_started == 0U) && (divisor != 1U))
        {
            text[index++] = ' ';
        }
        else
        {
            digit_started = 1U;
            text[index++] = (char)('0' + digit);
        }
    }

    text[index++] = '.';
    text[index++] = (char)('0' + (hundredths / 10U));
    text[index++] = (char)('0' + (hundredths % 10U));
    text[index++] = ' ';
    text[index++] = 's';
    text[index]   = '\0';
}

/* ---- 公开接口 ------------------------------------------------------------- */

/*
 * @brief  ISR 每 10ms 调用。
 *         初次进入时自动屏蔽 500ms（防起跑线误触发）。
 *         之后每 10ms 累加 LapTime10ms，每 100ms 触发一次 LCD 刷新。
 */
void LapTime_Tick10ms(void)
{
    if (lap_running != 0U)
    {
        if (was_running == 0U)
        {
            finish_line_ignore = 50U;  /* 500ms */
            was_running = 1U;
            LapTime10ms = 0U;
            display_tick = 0U;
            display_request = LAP_TIME_DISPLAY_RUNNING;
        }
        else
        {
            if (finish_line_ignore > 0U)
            {
                finish_line_ignore--;
            }

            if (LapTime10ms < UINT32_MAX)
            {
                LapTime10ms++;
            }

            display_tick++;
            if (display_tick >= 10U)  /* 100ms 刷新一次 */
            {
                display_tick = 0U;
                display_request = LAP_TIME_DISPLAY_RUNNING;
            }
        }
    }
    else
    {
        finish_line_ignore = 0U;
        was_running        = 0U;
        display_tick       = 0U;
    }
}

/*
 * @brief  起跑 500ms 屏蔽是否已过。
 */
bool LapTime_IsPastStartGuard(void)
{
    return (lap_running != 0U) && (finish_line_ignore == 0U);
}

/*
 * @brief  终点线触发时调用。
 */
void LapTime_MarkFinished(void)
{
    display_request = LAP_TIME_DISPLAY_FINISHED;
}

/*
 * @brief  主循环调用，处理 LCD 刷新请求。
 */
void LapTime_DisplayTask(void)
{
    uint8_t request;
    uint32_t time_10ms;
    uint32_t primask = __get_PRIMASK();
    char time_text[24];

    __disable_irq();
    request   = display_request;
    time_10ms = LapTime10ms;
    display_request = 0U;
    if (primask == 0U)
    {
        __enable_irq();
    }

    if (request == LAP_TIME_DISPLAY_RUNNING)
    {
        format_time(time_text, time_10ms);
        LCD_ShowString(LAP_TIME_DISPLAY_X,
                       LAP_TIME_DISPLAY_Y,
                       time_text,
                       YELLOW, BLACK, 24U, 0U);
    }
    else if (request == LAP_TIME_DISPLAY_FINISHED)
    {
        format_time(time_text, time_10ms);
        LCD_ShowString(LAP_TIME_DISPLAY_X,
                       LAP_TIME_DISPLAY_Y,
                       time_text,
                       GREEN, BLACK, 24U, 0U);
    }
}
