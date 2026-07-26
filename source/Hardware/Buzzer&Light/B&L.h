#ifndef __B_L_H
#define __B_L_H

#include <ti_msp_dl_config.h>

void BL_Init(void);

/* 蜂鸣器 PA11 */
void Buzzer_ON(void);
void Buzzer_OFF(void);
void Buzzer_Toggle(void);

/* 灯光 PA29 */
void Light_ON(void);
void Light_OFF(void);
void Light_Toggle(void);

#endif