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

        // BlueSerial_Printf("[plot,%.1f,%.1f,%.1f]", YawRate,turn_PID.Target,turn_PID.Out);
        // BlueSerial_Printf("[plot,%d,%d]", LeftPWM, RightPWM);
        //  BlueSerial_Printf("[plot,%.1f,%.1f]", AveSpeed,speed_PID.Target);
        // BlueSerial_Printf("[plot,%.1f,%.1f]", YawRate,turn_PID.Target);
        // BlueSerial_Printf("[plot,%.1f]", speed_PID.ErrorInt);
        // BlueSerial_Printf("[plot,%.1f]", turn_PID.ErrorInt);
        // BlueSerial_Printf("[plot,%.1f]", turn_PID.Out);
        // BlueSerial_Printf("[plot,%.1f,%.1f]",LineTracking_GetRawTarget(),YawRate);
        BlueSerial_Printf("[plot,%.1f]", AveSpeed);
    }
}


void TIMER_0_INST_IRQHandler(void)
{
    static uint8_t Count0;
    switch (DL_TimerG_getPendingInterrupt(TIMER_0_INST))
    {
    case DL_TIMER_IIDX_ZERO:
        YawRate = - Gyro_Data.gyro_z;
        flex_button_scan();

        if(LineTrackingFlag)
        {
            LineWalking();
            turn_PID.Target = LineTracking_GetError();
        }

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

// #endif /* Preserve the previous main.c implementation. */

// /*
//  * 8 路循迹数字量 LCD 测试
//  *
//  * T1 到 T8 按传感器从左到右排列，直接读取 GPIO 原始电平：
//  *   0 = 低电平（当前循迹逻辑约定为黑线）
//  *   1 = 高电平（当前循迹逻辑约定为白底）
//  */
// #include "ti_msp_dl_config.h"

// #include <stdint.h>

// #include "Hardware/Board/board.h"
// #include "Hardware/LCD/lcd.h"
// #include "Hardware/LCD/lcd_init.h"

// #define TRACKING_REFRESH_MS 50U
// #define TRACKING_CHANNELS   8U
// #define TRACKING_X_START    12U
// #define TRACKING_X_STEP     38U

// static void tracking_read_raw(uint8_t sensor[TRACKING_CHANNELS])
// {
//     sensor[0] = DL_GPIO_readPins(Track_T1_PORT, Track_T1_PIN) ? 1U : 0U;
//     sensor[1] = DL_GPIO_readPins(Track_T2_PORT, Track_T2_PIN) ? 1U : 0U;
//     sensor[2] = DL_GPIO_readPins(Track_T3_PORT, Track_T3_PIN) ? 1U : 0U;
//     sensor[3] = DL_GPIO_readPins(Track_T4_PORT, Track_T4_PIN) ? 1U : 0U;
//     sensor[4] = DL_GPIO_readPins(Track_T5_PORT, Track_T5_PIN) ? 1U : 0U;
//     sensor[5] = DL_GPIO_readPins(Track_T6_PORT, Track_T6_PIN) ? 1U : 0U;
//     sensor[6] = DL_GPIO_readPins(Track_T7_PORT, Track_T7_PIN) ? 1U : 0U;
//     sensor[7] = DL_GPIO_readPins(Track_T8_PORT, Track_T8_PIN) ? 1U : 0U;
// }






// #include "ti_msp_dl_config.h"
// #include "Hardware/Board/board.h"
// #include "Hardware/LCD/lcd.h"
// #include "Hardware/LCD/lcd_init.h"
// #include "Hardware/TB6612/bsp_tb6612.h"
// #include "Hardware/Encoder/Encoder.h"
// #include "middle/mid_debug_led.h"
// #include "middle/mid_pid.h"
// #include "app/app_ui.h"
// #include "app/app_key_task.h"
// #include "Hardware/BlueSerial/bsp_hc05_1.h"
// #include "Hardware/jy901s/jy901s.h"
// #include "app/app_irtracking.h"
// #include "Hardware/Buzzer&Light/B&L.h"

// volatile int16_t LeftPWM, RightPWM;
// int16_t AvePWM, DifPWM;
// volatile uint8_t RunFlag = 0U;
// volatile uint8_t LineTrackingFlag = 0U;
// float LeftSpeed, RightSpeed;
// float AveSpeed, DifSpeed;
// float YawRate;
// int16_t DeadZone = 0;

// PID_t speed_PID = {
//     .Kp = 2000,
//     .Ki = 400.0,
//     .Kd = 0,

//     .OutMax = 7000,
//     .OutMin = -7000,

//     .ErrorIntMax = 20,
//     .ErrorIntMin = -20,
// };

// PID_t turn_PID = {
//     .Kp = 60.0,
//     .Ki = 10.0,
//     .Kd = 150.0,

//     .OutMax = 5000,
//     .OutMin = -5000,

//     .ErrorIntMax = 450,
//     .ErrorIntMin = -450,
// };

// static void SystemIrq_Init(void)
// {
//     NVIC_ClearPendingIRQ(STEP_TIM_INST_INT_IRQN);
//     NVIC_EnableIRQ(STEP_TIM_INST_INT_IRQN);

//     NVIC_ClearPendingIRQ(TIMER_0_INST_INT_IRQN);
//     NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
// }

// static void Slave_ProcessBluetoothFrame(void)
// {
//     uint8_t command;
//     uint8_t data_length;
//     uint8_t control;

//     if (BLE_RX_FrameReady == 0U)
//     {
//         return;
//     }

//     command = BLE_RX_Frame.cmd;
//     data_length = BLE_RX_Frame.len;
//     control = (data_length > 0U) ? BLE_RX_Frame.data[0] : 0U;

//     /* The first valid frame represents a successful logical connection. */
//     Light_ON();

//     /* Toggle once for each complete frame that passed checksum validation. */
//     set_debug_led_toggle();

//     if ((command == CMD_START_STOP) &&
//         (data_length == 1U) &&
//         (control == PARAM_START))
//     {
//         LineTrackingFlag = 1U;
//         speed_PID.Target = 1.5f;
//         RunFlag = 1U;
//     }

//     BLERX_FLAG = 0U;
//     BLE_RX_FrameReady = 0U;
// }

// int main(void)
// {
//     SYSCFG_DL_init();
//     SystemIrq_Init();
//     Encoder_Init();
//     TB6612_Motor_Stop();
//     LCD_Init();
//     user_button_init();
//     Bluetooth_Init();
//     Gyro_Init();
//     BL_Init();
//     set_debug_led_off();

//     while (1)
//     {
//         Slave_ProcessBluetoothFrame();

//         if (LineTrackingFlag != 0U)
//         {
//             LineWalking();
//             turn_PID.Target = LineTracking_GetError();
//         }
//     }
// }

// void TIMER_0_INST_IRQHandler(void)
// {
//     static uint8_t Count0;

//     switch (DL_TimerG_getPendingInterrupt(TIMER_0_INST))
//     {
//         case DL_TIMER_IIDX_ZERO:
//             flex_button_scan();
//             YawRate = -Gyro_Data.gyro_z;

//             if (RunFlag != 0U)
//             {
//                 turn_PID.Actual = YawRate;
//                 PID_Update(&turn_PID);
//                 DifPWM = turn_PID.Out;

//                 LeftPWM = AvePWM + DifPWM;
//                 RightPWM = AvePWM - DifPWM;

//                 Motor_SetPWM(1, LeftPWM);
//                 Motor_SetPWM(2, RightPWM);
//             }

//             Count0++;
//             if (Count0 >= 4U)
//             {
//                 Count0 = 0U;
//                 Encoder_UpdateSpeed();
//                 LeftSpeed = Encoder_Get(0) * 25.0f / 780.0f;
//                 RightSpeed = Encoder_Get(1) * 25.0f / 780.0f;

//                 AveSpeed = (LeftSpeed + RightSpeed) / 2.0f;
//                 if (RunFlag != 0U)
//                 {
//                     speed_PID.Actual = AveSpeed;
//                     PID_Update(&speed_PID);
//                     AvePWM = speed_PID.Out;

//                     if (AvePWM > 0)
//                     {
//                         AvePWM += DeadZone;
//                     }
//                     else if (AvePWM < 0)
//                     {
//                         AvePWM -= DeadZone;
//                     }
//                 }
//             }
//             break;

//         default:
//             break;
//     }
// }

// int main(void)
// {
//     uint8_t sensor[TRACKING_CHANNELS];
//     uint8_t channel;

//     SYSCFG_DL_init();
//     LCD_Init();
//     LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, BLACK);

//     LCD_ShowString(8, 10, "TRACK DIGITAL TEST", YELLOW, BLACK, 24, 0);
//     LCD_ShowString(8, 48, "LEFT  <--- T1 TO T8 --->  RIGHT",
//                    CYAN, BLACK, 16, 0);

//     for (channel = 0U; channel < TRACKING_CHANNELS; channel++)
//     {
//         uint16_t x = TRACKING_X_START + (uint16_t)channel * TRACKING_X_STEP;
//         LCD_ShowChar(x, 76, 'T', WHITE, BLACK, 16, 0);
//         LCD_ShowChar(x + 8U, 76, (uint8_t)('1' + channel),
//                      WHITE, BLACK, 16, 0);
//     }

//     LCD_ShowString(8, 140, "0=BLACK   1=WHITE", GREEN, BLACK, 16, 0);

//     while (1)
//     {
//         tracking_read_raw(sensor);

//         for (channel = 0U; channel < TRACKING_CHANNELS; channel++)
//         {
//             uint16_t x = TRACKING_X_START + (uint16_t)channel * TRACKING_X_STEP;
//             LCD_ShowChar(x + 4U, 102, (uint8_t)('0' + sensor[channel]),
//                          GREEN, BLACK, 24, 0);
//         }

//         delay_ms(TRACKING_REFRESH_MS);
//     }
// }
