#ifndef __BSP_HC05_H__
#define __BSP_HC05_H__

#include "string.h"
#include "Hardware/Board/board.h"

// 是否开启串口0调试     1开始  0关闭
#define  DEBUG   0

#define  BLERX_LEN_MAX  200

#define  CONNECT             1       //蓝牙连接成功
#define  DISCONNECT          0       //蓝牙连接断开

#define  BLUETOOTH_LINK      CONNECT

extern unsigned char BLERX_BUFF[BLERX_LEN_MAX];
extern volatile unsigned char BLERX_FLAG;
extern volatile unsigned char BLERX_LEN;

void Bluetooth_Init(void);
unsigned char Get_Bluetooth_ConnectFlag(void);
void Bluetooth_Mode(void);
void Receive_Bluetooth_Data(void);
void BLE_send_String(unsigned char *str);
void Send_Bluetooth_Data(char *dat);
void Clear_BLERX_BUFF(void);
void Bluetooth_Printf(char *format, ...);
void BlueSerial_Printf(char *format, ...);
#endif