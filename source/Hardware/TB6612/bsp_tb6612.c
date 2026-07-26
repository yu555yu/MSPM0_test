#include "bsp_tb6612.h"
#include "Hardware/Board/board.h"
#define MOTOR_PWM_MAX 9999

void Motor_SetPWM(uint8_t motor, int32_t pwm)
{
    uint8_t dir = 1;
    uint32_t speed = 0;

    if(pwm >= 0)
    {
        dir = 1;
        speed = pwm;
    }
    else
    {
        dir = 0;
        speed = -pwm;
    }

    if(speed > MOTOR_PWM_MAX)
    {
        speed = MOTOR_PWM_MAX;
    }

    if(motor == 1)
    {
        AO_Control(dir, speed);
    }
    else if(motor == 2)
    {
        BO_Control(dir, speed);
    }
}

/******************************************************************
 * 函 数 名 称：TB6612_Motor_Stop
 * 函 数 说 明：A端和B端电机停止
 * 函 数 形 参：无
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：无
******************************************************************/
void TB6612_Motor_Stop(void)
{
    AIN1_OUT(1);
    AIN2_OUT(1);
    BIN1_OUT(1);
    BIN2_OUT(1);
}

/******************************************************************
 * 函 数 名 称：AO_Control
 * 函 数 说 明：A端口电机控制
 * 函 数 形 参：dir旋转方向 1正转0反转   speed旋转速度，范围（0 ~ per-1）
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：speed 0-10000
******************************************************************/
void AO_Control(uint8_t dir, uint32_t speed)
{
    if(speed > 9999 || dir > 1)
    {
        lc_printf("\nAO_Control parameter error!!!\r\n");
        return;
    }

    if( dir == 1 )
    {
        AIN1_OUT(1);
        AIN2_OUT(0);
    }
    else
    {
        AIN1_OUT(0);
        AIN2_OUT(1);
    }

    DL_TimerA_setCaptureCompareValue(PWM_0_INST, speed, GPIO_PWM_0_C1_IDX);
}



/******************************************************************
 * 函 数 名 称：BO_Control
 * 函 数 说 明：B端口电机控制
 * 函 数 形 参：dir旋转方向 1正转0反转   speed旋转速度，范围（0 ~ per-1）
 * 函 数 返 回：无
 * 作       者：LCKFB
 * 备       注：speed 0-10000
******************************************************************/
void BO_Control(uint8_t dir, uint32_t speed)
{
    if(speed > 9999 || dir > 1)
    {
        return;
    }

    if( dir == 1 )
    {
        BIN1_OUT(0);
        BIN2_OUT(1);
    }
    else
    {
        BIN1_OUT(1);
        BIN2_OUT(0);
    }

    DL_TimerA_setCaptureCompareValue(PWM_0_INST, speed, GPIO_PWM_0_C0_IDX);
}