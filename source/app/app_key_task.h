#ifndef _APP_KEY_TASK_H_
#define _APP_KEY_TASK_H_

#include "ti_msp_dl_config.h"
#include "middle/mid_button.h"

void set_app_key_current_mode(char mode);
char get_app_key_current_mode(void);

void btn_up_cb(flex_button_t *btn);
void btn_left_cb(flex_button_t *btn);
void btn_right_cb(flex_button_t *btn);
void btn_down_cb(flex_button_t *btn);
void btn_mid_cb(flex_button_t *btn);

/* 中键小球任务由main.c实现，按键层只负责启动或停止。 */
void BallMotionTask_Start(void);
void BallMotionTask_Stop(void);

#endif
