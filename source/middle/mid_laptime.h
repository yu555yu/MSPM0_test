#ifndef __MID_LAPTIME_H__
#define __MID_LAPTIME_H__

#include <stdint.h>
#include <stdbool.h>

/* ---- 圈速计时模块 --------------------------------------------------------- */

/* 由 ISR 每 10ms 调用一次，内部自动处理初始化 / 累加 / LCD 刷新请求。 */
void LapTime_Tick10ms(void);

/* 终点线触发时调用，显示最终成绩。 */
void LapTime_MarkFinished(void);

/* 主循环调用，处理 LCD 刷新。 */
void LapTime_DisplayTask(void);

/* 起跑后 500ms 屏蔽期是否已过 — 门控终点线检测。 */
bool LapTime_IsPastStartGuard(void);

#endif
