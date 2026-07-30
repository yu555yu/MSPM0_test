#if 0
#include "bsp_bluetooth_test_config.h"

#if BLUETOOTH_TEST_TARGET == BLUETOOTH_TEST_HC05_MASTER

#include "bsp_hc05_1.h"
#include "stdio.h"
#include <stdarg.h>

/* ---- HC-05 全局变量定义 -------------------------------------------------- */

unsigned char Bluetooth_ConnectFlag = 0;      /* 逻辑连接状态：0=未收到对端数据，1=已收到   */
unsigned char BLERX_BUFF[BLERX_LEN_MAX];     /* UART 接收缓冲区，ISR 填入                  */
volatile unsigned char BLERX_FLAG = 0;       /* 帧接收完成标志：ISR 置1，主循环查询后清零   */
volatile unsigned char BLERX_LEN = 0;        /* 当前帧有效字节数（不含'\0'）               */
volatile uint32_t Bluetooth_TestTxCount = 0; /* 测试用：累计发送 Ping / Ack 帧数            */
volatile uint32_t Bluetooth_TestRxCount = 0; /* 测试用：累计接收到有效帧数                  */
volatile uint32_t Bluetooth_RawRxCount = 0;  /* 诊断用：累计进入 UART RX 中断的原始字节数   */
volatile uint8_t Bluetooth_LastRxByte = 0;   /* 诊断用：最近一个 UART RX 原始字节           */

volatile BLE_Frame_t BLE_RX_Frame = {0};     /* 解析完成后的数据帧                         */
volatile uint8_t     BLE_RX_FrameReady = 0;  /* 新帧到达标志                               */

/******************************************************************
 * 函数名称：Bluetooth_VPrintf
 * 函数说明：蓝牙格式化发送内部函数
 * 函数形参：format 格式化字符串
 *              arg    可变参数列表
 * 函数返回：无
******************************************************************/
static void Bluetooth_VPrintf(char *format, va_list arg)
{
    char String[100];

    vsnprintf(String, sizeof(String), format, arg);

    Send_Bluetooth_Data(String);
}


/******************************************************************
 * 函数名称：Bluetooth_Printf
 * 函数说明：蓝牙格式化发送，类似 printf
 * 函数形参：format 格式化字符串
 * 函数返回：无
******************************************************************/
void Bluetooth_Printf(char *format, ...)
{
    va_list arg;

    va_start(arg, format);

    Bluetooth_VPrintf(format, arg);

    va_end(arg);
}


/******************************************************************
 * 函数名称：BlueSerial_Printf
 * 函数说明：兼容 STM32 BlueSerial_Printf 命名
 * 函数形参：format 格式化字符串
 * 函数返回：无
******************************************************************/
void BlueSerial_Printf(char *format, ...)
{
    va_list arg;

    va_start(arg, format);

    Bluetooth_VPrintf(format, arg);

    va_end(arg);
}

/******************************************************************
 * 函数名称：BLE_Send_Bit
 * 函数说明：向蓝牙发送单个字符
 * 函数形参：ch=ASCII字符
 * 函数返回：无
 * 作       者：LC
 * 备       注：无
******************************************************************/
void BLE_Send_Bit(unsigned char ch)
{
    // 当串口忙的时候等待，不忙的时候再发送传进来的字符
    while( DL_UART_isBusy(UART_0_INST) == true );
    // 发送单个字符
    DL_UART_Main_transmitData(UART_0_INST, ch);
}

/******************************************************************
 * 函数名称：BLE_send_String
 * 函数说明：向蓝牙发送字符串
 * 函数形参：str=发送的字符串
 * 函数返回：无
 * 作       者：LC
 * 备       注：无
******************************************************************/
void BLE_send_String(unsigned char *str)
{
    while( str && *str ) // 地址为空或者值为空跳出
    {
        BLE_Send_Bit(*str++);
    }
}

/******************************************************************
 * 函数名称：Clear_BLERX_BUFF
 * 函数说明：清除串口接收的数据
 * 函数形参：无
 * 函数返回：无
 * 作       者：LC
 * 备       注：无
******************************************************************/
void Clear_BLERX_BUFF(void)
{
    BLERX_LEN = 0;
    BLERX_FLAG = 0;
    BLERX_BUFF[0] = '\0';
}

/******************************************************************
 * 函数名称：Bluetooth_Init
 * 函数说明：蓝牙初始化
 * 函数形参：无
 * 函数返回：无
 * 作       者：LC
 * 备       注：默认波特率为9600
******************************************************************/
void Bluetooth_Init(void)
{
    Bluetooth_ConnectFlag = 0;
    Bluetooth_TestTxCount = 0;
    Bluetooth_TestRxCount = 0;
    Bluetooth_RawRxCount = 0;
    Bluetooth_LastRxByte = 0;
    Clear_BLERX_BUFF();

    // 清除串口中断标志
    NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
    // 使能串口中断
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);

    #if DEBUG
         // 在调试时，通过AT命令已经设置好模式
        lc_printf("Bluetooth_Init succeed!\r\n");

    #endif
}

/******************************************************************
 * 函数名称：Bluetooth_TestTask
 * 函数说明：HC-05 MASTER 裸板最小通信测试
 * 函数形参：tick_10ms 10ms系统节拍
 * 函数返回：无
 * 备       注：每秒发送 [MASTER:PING]；收到普通测试帧回复
 *              [MASTER:ACK]。另一端模块可接电脑 CH340。
******************************************************************/
void Bluetooth_TestTask(uint32_t tick_10ms)
{
    static uint32_t LastPingTick = 0;

    if((uint32_t)(tick_10ms - LastPingTick) >= 100U)
    {
        LastPingTick = tick_10ms;
        Bluetooth_Printf("[%s:PING]", HC05_DEVICE_NAME);
        Bluetooth_TestTxCount++;
    }

    if(BLERX_FLAG == 1U)
    {
        Bluetooth_TestRxCount++;
        Bluetooth_ConnectFlag = 1U;

        /* SLAVE 的正常反馈只记录，不再回复，避免两端 ACK 循环。*/
        if((strcmp((char *)BLERX_BUFF, "SLAVE:PONG") != 0) &&
           (strcmp((char *)BLERX_BUFF, "SLAVE:ACK") != 0))
        {
            Bluetooth_Printf("[%s:ACK]", HC05_DEVICE_NAME);
            Bluetooth_TestTxCount++;
        }

        Clear_BLERX_BUFF();
    }
}

/******************************************************************
 * 函数名称：Get_Bluetooth_ConnectFlag
 * 函数说明：获取手机连接状态
 * 函数形参：无
 * 函数返回：返回1=已连接                返回0=未连接
 * 作       者：LC
 * 备       注：使用该函数前，必须先调用 Bluetooth_Mode 函数
******************************************************************/
unsigned char Get_Bluetooth_ConnectFlag(void)
{
    return Bluetooth_ConnectFlag;
}

/******************************************************************
 * 函数名称：Bluetooth_Mode
 * 函数说明：判断蓝牙模块的连接状态
 * 函数形参：无
 * 函数返回：无
 * 作       者：LC
 * 备       注：未连接时STATE低电平   连接成功时STATE高电平
******************************************************************/
void Bluetooth_Mode(void)
{
    Bluetooth_ConnectFlag = 1;  // STATE 悬空，默认连接
}

/******************************************************************
 * 函数名称：Receive_Bluetooth_Data
 * 函数说明：接收蓝牙数据
 * 函数形参：无
 * 函数返回：无
 * 作       者：LC
 * 备       注：无
******************************************************************/
void Receive_Bluetooth_Data(void)
{
    if( BLERX_FLAG == 1 )// 接收到蓝牙数据
    {
        // 显示蓝牙发送过来的数据
        lc_printf("data = %s\r\n",BLERX_BUFF);
        Clear_BLERX_BUFF();// 清除接收缓存
    }
}

/******************************************************************
 * 函数名称：Send_Bluetooth_Data
 * 函数说明：向蓝牙模块发送数据
 * 函数形参：dat=要发送的字符串
 * 函数返回：无
 * 作       者：LC
 * 备       注：（如果手机连接了蓝牙，就是向手机发送数据）
******************************************************************/
void Send_Bluetooth_Data(char *dat)
{
    BLE_send_String((unsigned char*)dat);// 不用判断 STATE，直接发
}

/******************************************************************
 * 函数名称：BLE_BuildFrame
 * 函数说明：构造二进制数据帧
 * 帧格式：[0xFF][LEN][CMD][DATA...][CHK]
 *              LEN = CMD + DATA 字节数
 *              CHK = -(LEN+CMD+DATA...) & 0xFF
 * 函数返回：out 缓冲区中填充的总字节数
******************************************************************/
uint8_t BLE_BuildFrame(uint8_t cmd, const uint8_t *data, uint8_t len, uint8_t *out)
{
    uint8_t i;
    uint8_t checksum = 0;

    if (out == NULL || len > (BLERX_LEN_MAX - 4))
    {
        return 0;
    }

    out[0] = FRAME_HEAD;        /* 帧头 0xFF */
    out[1] = len + 1;           /* LEN = CMD + DATA */
    out[2] = cmd;

    checksum += out[1];
    checksum += out[2];

    for (i = 0; i < len; i++)
    {
        out[3 + i] = data[i];
        checksum += data[i];
    }

    out[3 + len] = (uint8_t)(-((int16_t)checksum));  /* 和校验补码 */

    return 3 + len + 1;         /* 0xFF + LEN + CMD + DATA + CHK */
}

/******************************************************************
 * 函数名称：BLE_SendFrame
 * 函数说明：构造并通过蓝牙 UART 发送二进制帧
******************************************************************/
void BLE_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    uint8_t tx_buf[BLERX_LEN_MAX];
    uint8_t tx_len;
    uint8_t i;

    tx_len = BLE_BuildFrame(cmd, data, len, tx_buf);
    for (i = 0; i < tx_len; i++)
    {
        BLE_Send_Bit(tx_buf[i]);
    }
}

/******************************************************************
 * 函数名称：BLE_SendStatus
 * 函数说明：发送状态/事件帧（如轨迹完成通知从车）
******************************************************************/
void BLE_SendStatus(uint8_t status)
{
    uint8_t data[1] = {status};
    BLE_SendFrame(CMD_STATUS, data, 1);
}

/******************************************************************
 * 函数名称：BLE_SendMotion
 * 函数说明：发送运动同步帧（AvePWM + DifPWM，小端）
******************************************************************/
void BLE_SendMotion(int16_t ave_pwm, int16_t dif_pwm)
{
    uint8_t data[4];
    data[0] = (uint8_t)(ave_pwm & 0xFF);
    data[1] = (uint8_t)((ave_pwm >> 8) & 0xFF);
    data[2] = (uint8_t)(dif_pwm & 0xFF);
    data[3] = (uint8_t)((dif_pwm >> 8) & 0xFF);
    BLE_SendFrame(CMD_MOTION, data, 4);
}

/******************************************************************
 * 函数名称：BLE_SendStartStop
 * 函数说明：发送启停控制帧
******************************************************************/
void BLE_SendStartStop(uint8_t start)
{
    uint8_t data[1] = {start ? PARAM_START : PARAM_STOP};
    BLE_SendFrame(CMD_START_STOP, data, 1);
}

/******************************************************************
 * 函数名称：BLE_SendHeartbeat
 * 函数说明：发送心跳帧
******************************************************************/
void BLE_SendHeartbeat(void)
{
    uint8_t data[1] = {0xAA};
    BLE_SendFrame(CMD_HEARTBEAT, data, 1);
}

// 串口的中断服务函数
void UART_0_INST_IRQHandler(void)
{
    static uint8_t RxState = 0;
    static uint8_t pRxPacket = 0;
    static uint8_t RxLen = 0;
    static uint8_t RxChecksum = 0;
    uint8_t RxData;

    switch(DL_UART_getPendingInterrupt(UART_0_INST))
    {
        case DL_UART_IIDX_RX:

            RxData = DL_UART_Main_receiveData(UART_0_INST);
            Bluetooth_LastRxByte = RxData;
            Bluetooth_RawRxCount++;

            /*
                二进制帧接收状态机：
                RxState = 0：等待帧头 0xFF
                RxState = 1：接收 LEN
                RxState = 2：接收 CMD + DATA
                RxState = 3：接收 CHK 并校验
            */
            if (RxState == 0)
            {
                if (RxData == FRAME_HEAD)
                {
                    RxState = 1;
                }
                /* 否则继续等待帧头 */
            }
            else if (RxState == 1)
            {
                RxLen = RxData;
                if (RxLen == 0 || RxLen > (BLERX_LEN_MAX - 3))
                {
                    /* 长度非法，丢弃 */
                    RxState = 0;
                }
                else
                {
                    RxChecksum = RxLen;
                    pRxPacket = 0;
                    RxState = 2;
                }
            }
            else if (RxState == 2)
            {
                BLERX_BUFF[pRxPacket++] = RxData;
                RxChecksum += RxData;

                if (pRxPacket >= RxLen)
                {
                    RxState = 3;
                }
            }
            else if (RxState == 3)
            {
                RxState = 0;

                /* 校验：LEN + CMD + DATA + CHK 之和应为 0 */
                if ((uint8_t)(RxChecksum + RxData) == 0U)
                {
                    if (BLE_RX_FrameReady == 0)
                    {
                        BLE_RX_Frame.cmd = BLERX_BUFF[0];
                        BLE_RX_Frame.len = RxLen - 1;

                        if (BLE_RX_Frame.len > sizeof(BLE_RX_Frame.data))
                        {
                            BLE_RX_Frame.len = sizeof(BLE_RX_Frame.data);
                        }

                        for (pRxPacket = 0; pRxPacket < BLE_RX_Frame.len; pRxPacket++)
                        {
                            BLE_RX_Frame.data[pRxPacket] = BLERX_BUFF[1 + pRxPacket];
                        }

                        BLE_RX_FrameReady = 1;
                        Bluetooth_TestRxCount++;
                        Bluetooth_ConnectFlag = 1;
                    }
                }
                /* 校验失败则丢弃 */
            }

            break;

        default:
            break;
    }
}

#endif
#endif /* Preserve the previous master implementation. */













// #include "bsp_hc05_1.h"

// #include <stdarg.h>
// #include <stdio.h>

// #include "Hardware/Board/board.h"

// volatile unsigned char Bluetooth_ConnectFlag = DISCONNECT;
// unsigned char BLERX_BUFF[BLERX_LEN_MAX];
// volatile unsigned char BLERX_FLAG = 0U;
// volatile unsigned char BLERX_LEN = 0U;
// volatile uint32_t Bluetooth_TestTxCount = 0U;
// volatile uint32_t Bluetooth_TestRxCount = 0U;
// volatile uint32_t Bluetooth_RawRxCount = 0U;
// volatile uint8_t Bluetooth_LastRxByte = 0U;

// volatile BLE_Frame_t BLE_RX_Frame = {0};
// volatile uint8_t BLE_RX_FrameReady = 0U;

// static void BLE_SendByte(uint8_t byte)
// {
//     while (DL_UART_isBusy(UART_0_INST) == true)
//     {
//     }

//     DL_UART_Main_transmitData(UART_0_INST, byte);
// }

// static void Bluetooth_VPrintf(char *format, va_list args)
// {
//     char string[100];

//     vsnprintf(string, sizeof(string), format, args);
//     Send_Bluetooth_Data(string);
// }

// void Bluetooth_Printf(char *format, ...)
// {
//     va_list args;

//     va_start(args, format);
//     Bluetooth_VPrintf(format, args);
//     va_end(args);
// }

// void BlueSerial_Printf(char *format, ...)
// {
//     va_list args;

//     va_start(args, format);
//     Bluetooth_VPrintf(format, args);
//     va_end(args);
// }

// void BLE_send_String(unsigned char *str)
// {
//     while ((str != NULL) && (*str != '\0'))
//     {
//         BLE_SendByte(*str);
//         str++;
//     }
// }

// void Send_Bluetooth_Data(char *dat)
// {
//     BLE_send_String((unsigned char *)dat);
// }

// void Clear_BLERX_BUFF(void)
// {
//     BLERX_LEN = 0U;
//     BLERX_FLAG = 0U;
//     BLE_RX_FrameReady = 0U;
//     BLERX_BUFF[0] = '\0';
// }

// void Bluetooth_Init(void)
// {
//     Bluetooth_ConnectFlag = DISCONNECT;
//     Bluetooth_TestTxCount = 0U;
//     Bluetooth_TestRxCount = 0U;
//     Bluetooth_RawRxCount = 0U;
//     Bluetooth_LastRxByte = 0U;
//     BLE_RX_Frame.cmd = 0U;
//     BLE_RX_Frame.len = 0U;
//     Clear_BLERX_BUFF();

//     NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
//     NVIC_EnableIRQ(UART_0_INST_INT_IRQN);
// }

// unsigned char Get_Bluetooth_ConnectFlag(void)
// {
//     return Bluetooth_ConnectFlag;
// }

// void Bluetooth_Mode(void)
// {
//     /* No STATE pin is connected. A valid received frame establishes the link. */
// }

// void Bluetooth_TestTask(uint32_t tick_10ms)
// {
//     /* The slave does not send PING, ACK, or any unsolicited data. */
//     (void)tick_10ms;
// }

// void Receive_Bluetooth_Data(void)
// {
//     if (BLE_RX_FrameReady == 1U)
//     {
//         Clear_BLERX_BUFF();
//     }
// }

// uint8_t BLE_BuildFrame(uint8_t cmd, const uint8_t *data, uint8_t len, uint8_t *out)
// {
//     uint8_t checksum = 0U;
//     uint8_t index;

//     if ((out == NULL) || (len > (BLERX_LEN_MAX - 4U)))
//     {
//         return 0U;
//     }

//     out[0] = FRAME_HEAD;
//     out[1] = (uint8_t)(len + 1U);
//     out[2] = cmd;

//     checksum = (uint8_t)(out[1] + out[2]);
//     for (index = 0U; index < len; index++)
//     {
//         out[3U + index] = data[index];
//         checksum = (uint8_t)(checksum + data[index]);
//     }

//     out[3U + len] = (uint8_t)(0U - checksum);
//     return (uint8_t)(4U + len);
// }

// void BLE_SendFrame(uint8_t cmd, const uint8_t *data, uint8_t len)
// {
//     uint8_t tx_buffer[BLERX_LEN_MAX];
//     uint8_t tx_length;
//     uint8_t index;

//     tx_length = BLE_BuildFrame(cmd, data, len, tx_buffer);
//     for (index = 0U; index < tx_length; index++)
//     {
//         BLE_SendByte(tx_buffer[index]);
//     }
// }

// void BLE_SendStatus(uint8_t status)
// {
//     BLE_SendFrame(CMD_STATUS, &status, 1U);
// }

// void BLE_SendMotion(int16_t ave_pwm, int16_t dif_pwm)
// {
//     uint8_t data[4];

//     data[0] = (uint8_t)(ave_pwm & 0xFF);
//     data[1] = (uint8_t)(((uint16_t)ave_pwm >> 8U) & 0xFFU);
//     data[2] = (uint8_t)(dif_pwm & 0xFF);
//     data[3] = (uint8_t)(((uint16_t)dif_pwm >> 8U) & 0xFFU);
//     BLE_SendFrame(CMD_MOTION, data, 4U);
// }

// void BLE_SendStartStop(uint8_t start)
// {
//     uint8_t data = (start != 0U) ? PARAM_START : PARAM_STOP;

//     BLE_SendFrame(CMD_START_STOP, &data, 1U);
// }

// void BLE_SendHeartbeat(void)
// {
//     uint8_t data = 0xAAU;

//     BLE_SendFrame(CMD_HEARTBEAT, &data, 1U);
// }

// void UART_0_INST_IRQHandler(void)
// {
//     static uint8_t rx_state = 0U;
//     static uint8_t rx_index = 0U;
//     static uint8_t rx_length = 0U;
//     static uint8_t rx_checksum = 0U;
//     uint8_t rx_data;
//     uint8_t index;

//     switch (DL_UART_getPendingInterrupt(UART_0_INST))
//     {
//         case DL_UART_IIDX_RX:
//             rx_data = DL_UART_Main_receiveData(UART_0_INST);
//             Bluetooth_LastRxByte = rx_data;
//             Bluetooth_RawRxCount++;

//             if (rx_state == 0U)
//             {
//                 if (rx_data == FRAME_HEAD)
//                 {
//                     rx_state = 1U;
//                 }
//             }
//             else if (rx_state == 1U)
//             {
//                 rx_length = rx_data;
//                 if ((rx_length == 0U) ||
//                     (rx_length > (BLERX_LEN_MAX - 3U)))
//                 {
//                     rx_state = 0U;
//                 }
//                 else
//                 {
//                     rx_checksum = rx_length;
//                     rx_index = 0U;
//                     rx_state = 2U;
//                 }
//             }
//             else if (rx_state == 2U)
//             {
//                 BLERX_BUFF[rx_index] = rx_data;
//                 rx_index++;
//                 rx_checksum = (uint8_t)(rx_checksum + rx_data);

//                 if (rx_index >= rx_length)
//                 {
//                     rx_state = 3U;
//                 }
//             }
//             else
//             {
//                 rx_state = 0U;

//                 if ((uint8_t)(rx_checksum + rx_data) == 0U)
//                 {
//                     Bluetooth_ConnectFlag = CONNECT;

//                     if (BLE_RX_FrameReady == 0U)
//                     {
//                         BLE_RX_Frame.cmd = BLERX_BUFF[0];
//                         BLE_RX_Frame.len = (uint8_t)(rx_length - 1U);

//                         for (index = 0U; index < BLE_RX_Frame.len; index++)
//                         {
//                             BLE_RX_Frame.data[index] = BLERX_BUFF[index + 1U];
//                         }

//                         BLERX_LEN = rx_length;
//                         BLERX_FLAG = 1U;
//                         BLE_RX_FrameReady = 1U;
//                         Bluetooth_TestRxCount++;
//                     }
//                 }
//             }
//             break;

//         default:
//             break;
//     }
// }
