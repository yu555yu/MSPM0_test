#include "KEY.h"
//按键读取功能
//返回0时代表按下
//返回1时表示没有按下
KEY_STATUS key_scan(void)
{
    KEY_STATUS states;
    // 读取每个按键的状态（0=按下，1=未按下）
    states.up    = DL_GPIO_readPins(KEY_KEY_UP_PORT,    KEY_KEY_UP_PIN)    ? 1 : 0;
    states.left  = DL_GPIO_readPins(KEY_KEY_LEFT_PORT,  KEY_KEY_LEFT_PIN)  ? 1 : 0;
    states.right = DL_GPIO_readPins(KEY_KEY_RIGHT_PORT, KEY_KEY_RIGHT_PIN) ? 1 : 0;
    states.down  = 0;   /* DOWN 已弃用，始终未按下 */
    states.mid   = DL_GPIO_readPins(KEY_KEY_MID_PORT,   KEY_KEY_MID_PIN)   ? 1 : 0;

    return states;
}