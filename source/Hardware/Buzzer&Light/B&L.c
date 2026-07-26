#include "B&L.h"

void BL_Init(void)
{
    DL_GPIO_clearPins(Buzzer_PORT, Buzzer_PIN_11_PIN);
    DL_GPIO_clearPins(Light_PORT, Light_PIN_29_PIN);
}

/* 蜂鸣器 PA11 */
void Buzzer_ON(void)     { DL_GPIO_setPins   (Buzzer_PORT, Buzzer_PIN_11_PIN); }
void Buzzer_OFF(void)    { DL_GPIO_clearPins (Buzzer_PORT, Buzzer_PIN_11_PIN); }
void Buzzer_Toggle(void) { DL_GPIO_togglePins(Buzzer_PORT, Buzzer_PIN_11_PIN); }

/* 灯光 PA29 */
void Light_ON(void)      { DL_GPIO_setPins   (Light_PORT, Light_PIN_29_PIN); }
void Light_OFF(void)     { DL_GPIO_clearPins (Light_PORT, Light_PIN_29_PIN); }
void Light_Toggle(void)  { DL_GPIO_togglePins(Light_PORT, Light_PIN_29_PIN); }
