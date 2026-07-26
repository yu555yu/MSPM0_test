#include "ti_msp_dl_config.h"
#include "Hardware/Board/board.h"
#include "Hardware/LCD/lcd.h"
#include "Hardware/LCD/lcd_init.h"
#include "Hardware/TB6612/bsp_tb6612.h"
#include "Hardware/Encoder/Encoder.h"
#include "middle/mid_debug_led.h"
#include "middle/mid_pid.h"
#include "app/app_ui.h"
#include "app/app_key_task.h"
#include "Hardware/BlueSerial/bsp_hc05.h"
#include <stdlib.h>
#include "Hardware/jy901s/jy901s.h"
#include "app/app_irtracking.h"
#include "Hardware/Buzzer&Light/B&L.h"

volatile int16_t LeftPWM , RightPWM;
int16_t AvePWM , DifPWM;
volatile uint8_t RunFlag = 0;
volatile uint8_t LineTrackingFlag = 0;
float LeftSpeed , RightSpeed;
float AveSpeed , DifSpeed;
float YawRate;   // Z轴角速度（°/s）
int16_t DeadZone = 0;

PID_t speed_PID ={
	.Kp = 2000,
	.Ki = 400.0,
	.Kd = 0,

    .OutMax = 7000,
    .OutMin = -7000,

    .ErrorIntMax = 20,
    .ErrorIntMin = -20,
};

PID_t turn_PID ={
	.Kp = 60.0,
	.Ki = 8.5,
	.Kd = 150.0,

	.OutMax = 5000,
	.OutMin = -5000,

    .ErrorIntMax = 420,
    .ErrorIntMin = -420,
};

static void SystemIrq_Init(void)
{
    NVIC_ClearPendingIRQ(STEP_TIM_INST_INT_IRQN);
    NVIC_EnableIRQ(STEP_TIM_INST_INT_IRQN);

    NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
}

int main(void)
{
    SYSCFG_DL_init();
    SystemIrq_Init();
    Encoder_Init();
    TB6612_Motor_Stop();
    LCD_Init();
    user_button_init();
    Bluetooth_Init();
    Gyro_Init();
    BL_Init();
    
    // LCD_BLK_Clr();
    // LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, BLACK);
    // LCD_BLK_Set();
    while (1)
    {
        if (BLERX_FLAG == 1)
        {
             set_debug_led_toggle();
             char *Tag = strtok((char *)BLERX_BUFF, ",");
                if (strcmp(Tag, "key") == 0)
				{
					// char *Name = strtok(NULL, ",");
					// char *Action = strtok(NULL, ",");
				}
                else if (strcmp(Tag, "slider") == 0)
				{
					char *Name = strtok(NULL, ",");
					char *Value = strtok(NULL, ",");

                    if(strcmp(Name, "SpeedKp") == 0)
					{
						speed_PID.Kp = atof(Value);
					}
					else if(strcmp(Name, "SpeedKi") == 0)
					{
						speed_PID.Ki = atof(Value);
					}
					else if(strcmp(Name, "SpeedKd") == 0)
					{
						speed_PID.Kd = atof(Value);
					}
                    else if(strcmp(Name, "TurnKp") == 0)
					{
						turn_PID.Kp = atof(Value);
					}
					else if(strcmp(Name, "TurnKi") == 0)
					{
						turn_PID.Ki = atof(Value);
					}
					else if(strcmp(Name, "TurnKd") == 0)
					{
						turn_PID.Kd = atof(Value);
					}
                }
                else if (strcmp(Tag, "joystick") == 0)
				{
					int8_t LH = atoi(strtok(NULL, ","));
					int8_t LV = atoi(strtok(NULL, ","));
					int8_t RH = atoi(strtok(NULL, ","));
					int8_t RV = atoi(strtok(NULL, ","));
					(void)LH;
                    (void)RV;
					speed_PID.Target = LV / 40.0;
                    turn_PID.Target = RH * 0.9;
                    RunFlag = 1;
				}
            BLERX_FLAG = 0;
        }

        if (LineTrackingFlag)
        {
            LineWalking();
            turn_PID.Target = LineTracking_GetError();
        }

        // BlueSerial_Printf("[plot,%.1f,%.1f,%.1f]", YawRate,turn_PID.Target,turn_PID.Out);
        BlueSerial_Printf("[plot,%d,%d]", LeftPWM, RightPWM);
        //  BlueSerial_Printf("[plot,%.1f,%.1f]", AveSpeed,speed_PID.Target);
        // BlueSerial_Printf("[plot,%.1f,%.1f]", YawRate,turn_PID.Target);
        // BlueSerial_Printf("[plot,%.1f]", speed_PID.ErrorInt);
        // BlueSerial_Printf("[plot,%.1f]", turn_PID.ErrorInt);
        // BlueSerial_Printf("[plot,%.1f]", turn_PID.Out);
        /* current angle, target angle, shortest-path error, yaw-rate target */
        // BlueSerial_Printf("[plot,%.1f,%.1f,%.1f,%.1f]",
        //                   -Gyro_Data.angle_z,
        //                   AnglePID_GetTarget(&angle_pid),
        //                   AnglePID_GetError(&angle_pid),
        //                   angle_pid.output);
        //  BlueSerial_Printf("[plot,%.1f]", -Gyro_Data.angle_z);
    }
}


void TIMER_0_INST_IRQHandler(void)
{
    static uint8_t Count0;
    switch (DL_TimerG_getPendingInterrupt(TIMER_0_INST))
    {
    case DL_TIMER_IIDX_ZERO:
        flex_button_scan();
        YawRate = - Gyro_Data.gyro_z;
        if(RunFlag)
        {
            turn_PID.Actual = YawRate;
            PID_Update(&turn_PID);
            DifPWM = turn_PID.Out;

            LeftPWM  = AvePWM + DifPWM;
            RightPWM = AvePWM - DifPWM;

            Motor_SetPWM(1 , LeftPWM);
			Motor_SetPWM(2 , RightPWM);
        }
        Count0 ++;
        if(Count0 >= 4)
		{
            Count0 = 0;
            Encoder_UpdateSpeed();
            //转速
            LeftSpeed = Encoder_Get(0) * 25.0f / 780.0f;
            RightSpeed = Encoder_Get(1) * 25.0f / 780.0f;

            AveSpeed = (LeftSpeed + RightSpeed) / 2.0;
            if(RunFlag)
			{
				speed_PID.Actual = AveSpeed;
				PID_Update(&speed_PID);
				AvePWM = speed_PID.Out;

				if (AvePWM > 0) AvePWM += DeadZone;
				else if (AvePWM < 0) AvePWM -= DeadZone;
			}
        }
        break;
    default:
        break;
    }
}
