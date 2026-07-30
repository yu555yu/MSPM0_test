#include "app_key_task.h"
#include "middle/mid_debug_led.h"
#include "middle/mid_pid.h"
#include "Hardware/TB6612/bsp_tb6612.h"
#include "Hardware/Motor/stepper_motor.h"  
#include "Hardware/Buzzer&Light/B&L.h"    


extern volatile int16_t LeftPWM;
extern volatile int16_t RightPWM;
extern int16_t AvePWM;
extern int16_t DifPWM;
extern volatile uint8_t RunFlag;
extern volatile uint8_t LineTrackingFlag;

extern PID_t speed_PID;
extern PID_t turn_PID;


//--------------------------向上的按键-------------------------------
void btn_up_cb(flex_button_t *btn)
{
    switch (btn->event)
    {
        case FLEX_BTN_PRESS_DOWN:
            set_debug_led_toggle();
            LineTrackingFlag = 1;
            speed_PID.Target = 1.2f;
            RunFlag = 1;
            break;

        case FLEX_BTN_PRESS_LONG_HOLD:
            break;

        case FLEX_BTN_PRESS_LONG_HOLD_UP:
            break;

        default:
            break;
    }
}


void btn_down_cb(flex_button_t *btn)
{
    (void)btn;
}


//--------------------------向左的按键-------------------------------
void btn_left_cb(flex_button_t *btn)
{
    switch (btn->event)
    {
        case FLEX_BTN_PRESS_DOWN:
            set_debug_led_toggle();
            break;

        default:
            break;
    }
}


//--------------------------向右的按键-------------------------------
void btn_right_cb(flex_button_t *btn)
{
    switch (btn->event)
    {
        case FLEX_BTN_PRESS_CLICK:
            set_debug_led_toggle();
            break;

        default:
            break;
    }
}


//--------------------------中间的按键-------------------------------
void btn_mid_cb(flex_button_t *btn)
{
    switch (btn->event)
    {
        case FLEX_BTN_PRESS_DOWN:
            set_debug_led_toggle();
            break;

        case FLEX_BTN_PRESS_LONG_HOLD:
            break;

        case FLEX_BTN_PRESS_LONG_HOLD_UP:
            break;

        default:
            break;
    }
}
