// #ifndef __BSP_HC04_H__
// #define __BSP_HC04_H__

// #include <stdint.h>
// #include "string.h"
// #include "Hardware/Board/board.h"

// /*===========================================================================
//  * HC-04 蓝牙模块驱动 —— 固定安装在从车
//  *
//  * 模块参数（数据手册 V2.6 / 2023-12-12）：
//  *   SPP + BLE 5.0 双模  |  默认波特率 9600/8N1
//  *   默认密码 1234        |  默认名称 HC-04 (SPP) / HC-04BLE (BLE)
//  *   AT 模式进入：KEY+ 引脚拉低 250~300ms → 保持 9600bps
//  *
//  * 本文件作用：
//  *   SLAVE 角色，被动等待主机 Ping 并回复 Pong/Ack。
//  *   当前采用预配置模式：模块出厂后用 PC+CH340 通过 AT 命令完成
//  *   角色/模式/波特率设置，MCU 上电后直接透传通信，不发送 AT 指令。
//  *===========================================================================*/

// /* ---- 宏定义 --------------------------------------------------------------- */

// /* 模块身份：HC-04 固定为从机（从车） */
// #define SLAVE                 2U          /* 从模式标识                        */
// #define HC04_DEVICE_ROLE      SLAVE       /* 本模块角色：从                    */
// #define HC04_DEVICE_NAME      "SLAVE"    /* 测试帧中显示的身份名              */

// /* 调试开关：1=通过 lc_printf 输出初始化日志；0=静默 */
// #define DEBUG                 0

// /* 接收缓冲区最大字节数（含结尾 '\0'），SPP 单包通常不超过此值 */
// #define BLERX_LEN_MAX         200

// /* 蓝牙逻辑连接状态 */
// #define CONNECT               1           /* 已连接 / 已收到对端数据           */
// #define DISCONNECT            0           /* 未连接 / 未收到对端数据           */

// /*
//  * BLUETOOTH_LINK：编译期期望的链路状态。
//  * 当前固定为 CONNECT，表示上电即认为模块已配对完成。
//  */
// #define BLUETOOTH_LINK        CONNECT

// /* ---- 全局变量声明 --------------------------------------------------------- */

// extern unsigned char BLERX_BUFF[BLERX_LEN_MAX];   /* 接收缓冲区，ISR 写入     */
// extern volatile unsigned char BLERX_FLAG;          /* =1 表示一帧接收完成       */
// extern volatile unsigned char BLERX_LEN;           /* 当前帧有效字节数           */
// extern volatile uint32_t Bluetooth_TestTxCount;    /* 测试：累计发送帧数         */
// extern volatile uint32_t Bluetooth_TestRxCount;    /* 测试：累计接收帧数         */
// extern volatile uint32_t Bluetooth_RawRxCount;     /* 诊断：UART 原始接收字节数  */
// extern volatile uint8_t Bluetooth_LastRxByte;      /* 诊断：最近收到的原始字节   */

// /* ---- 对外函数声明 --------------------------------------------------------- */

// void Bluetooth_Init(void);                          /* 初始化 UART 与全局变量    */
// unsigned char Get_Bluetooth_ConnectFlag(void);      /* 查询是否收到过对端数据    */
// void Bluetooth_Mode(void);                          /* 兼容接口，预留 STATE 检测 */
// void Bluetooth_TestTask(uint32_t tick_10ms);        /* 裸板测试：Ping/Pong 逻辑  */
// void Receive_Bluetooth_Data(void);                  /* 调试：打印接收内容并清除  */
// void BLE_send_String(unsigned char *str);           /* 发送 C 字符串             */
// void Send_Bluetooth_Data(char *dat);                /* 封装发送，供上层调用       */
// void Clear_BLERX_BUFF(void);                        /* 清空接收缓冲区与标志       */
// void Bluetooth_Printf(char *format, ...);           /* 蓝牙格式化发送            */
// void BlueSerial_Printf(char *format, ...);          /* 兼容 STM32 命名           */

// #endif
