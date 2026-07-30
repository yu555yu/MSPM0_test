#if 0
#ifndef __BSP_HC05_1_H__
#define __BSP_HC05_1_H__

#include <stdint.h>
#include "string.h"
#include "Hardware/Board/board.h"

/*===========================================================================
 * HC-05 蓝牙模块驱动 —— 固定安装在主车
 *
 * 模块参数（数据手册 V2.1）：
 *   蓝牙 2.0 EDR  |  CSR BC417  |  默认波特率 9600/8N1
 *   默认密码 1234  |  默认名称 HC-05
 *   AT 模式进入：PIO11 拉高后上电 → 38400bps 固定
 *
 * 本文件作用：
 *   MASTER 角色，负责主动 Ping 从机并处理从机应答。
 *   当前采用预配置模式：模块出厂后用 PC+CH340 通过 AT 命令完成
 *   角色/绑定/波特率设置，MCU 上电后直接透传通信，不发送 AT 指令。
 *===========================================================================*/

/* ---- 宏定义 --------------------------------------------------------------- */

/* 模块身份：HC-05 固定为主机（主车）*/
#define MASTER                1U          /* 主模式标识                       */
#define HC05_DEVICE_ROLE      MASTER      /* 本模块角色：主                   */
#define HC05_DEVICE_NAME      "MASTER"   /* 测试帧中显示的身份名              */

/* 调试开关：1=通过 lc_printf 输出初始化日志；0=静默 */
#define DEBUG                 0

/* 接收缓冲区最大字节数（含结尾 '\0'），SPP 单包通常不超过此值 */
#define BLERX_LEN_MAX         200

/* 蓝牙逻辑连接状态 */
#define CONNECT               1           /* 已连接 / 已收到对端数据           */
#define DISCONNECT            0           /* 未连接 / 未收到对端数据           */

/*
 * BLUETOOTH_LINK：编译时期望的链路状态。
 * 当前固定为 CONNECT，表示上电即认为模块已配对完成。
 */
#define BLUETOOTH_LINK        CONNECT

/* ---- 双车通信二进制帧协议 ------------------------------------------------- */

#define FRAME_HEAD            0xFF        /* 帧头                              */
#define FRAME_HEAD_LEN        1           /* 帧头字节数                        */
#define FRAME_EXTRA_LEN       2           /* 帧头 + 校验，不计入 LEN           */
#define FRAME_MIN_LEN         4           /* 0xFF + LEN + CMD + CHK            */

/* 命令字 */
#define CMD_START_STOP        0x01        /* 启停控制                          */
#define CMD_MOTION            0x02        /* 运动同步（AvePWM + DifPWM）       */
#define CMD_STATUS            0x03        /* 状态/事件报告                     */
#define CMD_HEARTBEAT         0x04        /* 心跳                              */

/* CMD_STATUS 的状态码 */
#define STATUS_IDLE           0x00        /* 待机                              */
#define STATUS_RUNNING        0x01        /* 运行中                            */
#define STATUS_DONE           0x02        /* 主车轨迹完成，通知从车启动         */
#define STATUS_ESTOP          0xFF        /* 急停                              */

/* CMD_START_STOP 的参数 */
#define PARAM_STOP            0x00
#define PARAM_START           0x01

/* ---- 全局变量声明 --------------------------------------------------------- */

extern unsigned char BLERX_BUFF[BLERX_LEN_MAX];   /* 接收缓冲区，ISR 写入      */
extern volatile unsigned char BLERX_FLAG;          /* =1 表示一帧接收完成       */
extern volatile unsigned char BLERX_LEN;           /* 当前帧有效字节数           */
extern volatile uint32_t Bluetooth_TestTxCount;    /* 测试：累计发送帧数         */
extern volatile uint32_t Bluetooth_TestRxCount;    /* 测试：累计接收帧数         */
extern volatile uint32_t Bluetooth_RawRxCount;     /* 诊断：UART 原始接收字节数  */
extern volatile uint8_t Bluetooth_LastRxByte;      /* 诊断：最近收到的原始字节    */

/* ---- 数据帧结构 ----------------------------------------------------------- */

typedef struct
{
    uint8_t  cmd;                                   /* 命令字                    */
    uint8_t  len;                                   /* 数据长度                  */
    uint8_t  data[BLERX_LEN_MAX - 3];               /* 数据域（预留）            */
} BLE_Frame_t;

extern volatile BLE_Frame_t BLE_RX_Frame;           /* 解析完成后的数据帧         */
extern volatile uint8_t      BLE_RX_FrameReady;     /* 新帧到达标志               */

/* ---- 对外函数声明 --------------------------------------------------------- */

void Bluetooth_Init(void);                          /* 初始化 UART 与全局变量     */
unsigned char Get_Bluetooth_ConnectFlag(void);      /* 查询是否收到过对端数据     */
void Bluetooth_Mode(void);                          /* 兼容接口，预留 STATE 检测  */
void Bluetooth_TestTask(uint32_t tick_10ms);        /* 裸板测试：Ping/Pong 逻辑   */
void Receive_Bluetooth_Data(void);                  /* 调试：打印接收内容并清除    */
void BLE_send_String(unsigned char *str);           /* 发送 C 字符串              */
void Send_Bluetooth_Data(char *dat);                /* 封装发送，供上层调用        */
void Clear_BLERX_BUFF(void);                        /* 清空接收缓冲区与标志        */
void Bluetooth_Printf(char *format, ...);           /* 蓝牙格式化发送              */
void BlueSerial_Printf(char *format, ...);          /* 兼容 STM32 命名            */

uint8_t BLE_BuildFrame(uint8_t cmd, const uint8_t *data, uint8_t len, uint8_t *out);
void    BLE_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t len);
void    BLE_SendStatus(uint8_t status);
void    BLE_SendMotion(int16_t ave_pwm, int16_t dif_pwm);
void    BLE_SendStartStop(uint8_t start);
void    BLE_SendHeartbeat(void);

#endif
#endif /* Preserve the previous master header. */














// #ifndef __BSP_HC05_1_H__
// #define __BSP_HC05_1_H__

// #include <stdint.h>

// #define HC05_ROLE_SLAVE       0U
// #define HC05_DEVICE_ROLE      HC05_ROLE_SLAVE
// #define HC05_DEVICE_NAME      "SLAVE"

// #define DEBUG                 0
// #define BLERX_LEN_MAX         200U

// #define CONNECT               1U
// #define DISCONNECT            0U

// /* Binary frame: [HEAD][LEN][CMD][DATA...][CHK].
//  * LEN = CMD bytes + DATA bytes.
//  * LEN + CMD + DATA + CHK must equal 0 modulo 256.
//  */
// #define FRAME_HEAD            0xFFU
// #define FRAME_HEAD_LEN        1U
// #define FRAME_EXTRA_LEN       2U
// #define FRAME_MIN_LEN         4U

// #define CMD_START_STOP        0x01U
// #define CMD_MOTION            0x02U
// #define CMD_STATUS            0x03U
// #define CMD_HEARTBEAT         0x04U

// #define STATUS_IDLE           0x00U
// #define STATUS_RUNNING        0x01U
// #define STATUS_DONE           0x02U
// #define STATUS_ESTOP          0xFFU

// #define PARAM_STOP            0x00U
// #define PARAM_START           0x01U

// typedef struct
// {
//     uint8_t cmd;
//     uint8_t len;
//     uint8_t data[BLERX_LEN_MAX - 3U];
// } BLE_Frame_t;

// extern unsigned char BLERX_BUFF[BLERX_LEN_MAX];
// extern volatile unsigned char Bluetooth_ConnectFlag;
// extern volatile unsigned char BLERX_FLAG;
// extern volatile unsigned char BLERX_LEN;
// extern volatile uint32_t Bluetooth_TestTxCount;
// extern volatile uint32_t Bluetooth_TestRxCount;
// extern volatile uint32_t Bluetooth_RawRxCount;
// extern volatile uint8_t Bluetooth_LastRxByte;

// extern volatile BLE_Frame_t BLE_RX_Frame;
// extern volatile uint8_t BLE_RX_FrameReady;

// void Bluetooth_Init(void);
// unsigned char Get_Bluetooth_ConnectFlag(void);
// void Bluetooth_Mode(void);
// void Bluetooth_TestTask(uint32_t tick_10ms);
// void Receive_Bluetooth_Data(void);
// void BLE_send_String(unsigned char *str);
// void Send_Bluetooth_Data(char *dat);
// void Clear_BLERX_BUFF(void);
// void Bluetooth_Printf(char *format, ...);
// void BlueSerial_Printf(char *format, ...);

// uint8_t BLE_BuildFrame(uint8_t cmd, const uint8_t *data, uint8_t len, uint8_t *out);
// void BLE_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t len);
// void BLE_SendStatus(uint8_t status);
// void BLE_SendMotion(int16_t ave_pwm, int16_t dif_pwm);
// void BLE_SendStartStop(uint8_t start);
// void BLE_SendHeartbeat(void);

// #endif
