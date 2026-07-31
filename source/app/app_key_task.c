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
extern volatile uint8_t lap_running;
extern volatile uint8_t BallBalanceControlEnabled;
extern volatile float BallLiftTargetMm;

extern PID_t speed_PID;
extern PID_t turn_PID;
extern PID_t ball_position_PID;


//--------------------------向上的按键-------------------------------
void btn_up_cb(flex_button_t *btn)
{
    switch (btn->event)
    {
        case FLEX_BTN_PRESS_DOWN:
            if (lap_running != 0U) break;  /* 正在跑圈 → 忽略 */
            set_debug_led_toggle();
            lap_running = 1;
            LineTrackingFlag = 1;
            speed_PID.Target = 2.0f;
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
            /* 重新记录零点前先停止小球闭环，防止标定过程中电机动作。 */
            BallBalanceControlEnabled = 0U;
            BallLiftTargetMm = 0.0f;
            BallMotionTask_Stop();
            if_recore_balance = true;
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
        /*
         * 使用按下事件立即启动，避免按键释放或消抖过程没有生成CLICK事件，
         * 导致已经记录平衡点却始终无法使能小球闭环。
         */
        case FLEX_BTN_PRESS_DOWN:
            set_debug_led_toggle();

            /*
             * 只有左键已成功建立平衡零点后才允许启动小球闭环。
             * 启动时清空PID历史，目标设为管子中心0cm。
             */
            if (LiftMotor_BalanceDetect())
            {
                /* 从中心出发：先去+5cm，再折返到-5cm并保持。 */
                BallMotionTask_Start();
            }
            break;

        case FLEX_BTN_PRESS_LONG_HOLD:
            break;

        case FLEX_BTN_PRESS_LONG_HOLD_UP:
            break;

        default:
            break;
    }
}
