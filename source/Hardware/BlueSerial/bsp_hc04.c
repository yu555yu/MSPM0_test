// #include "bsp_bluetooth_test_config.h"

// #if BLUETOOTH_TEST_TARGET == BLUETOOTH_TEST_HC04_SLAVE

// #include "bsp_hc04.h"
// #include "stdio.h"
// #include <stdarg.h>

// /* ---- HC-04 全局变量定义 -------------------------------------------------- */

// unsigned char Bluetooth_ConnectFlag = 0;      /* 逻辑连接状态：0=未收到对端数据，1=已收到  */
// unsigned char BLERX_BUFF[BLERX_LEN_MAX];     /* UART 接收缓冲区，ISR 填入                    */
// volatile unsigned char BLERX_FLAG = 0;       /* 帧接收完成标志：ISR 置 1，主循环查询后清零    */
// volatile unsigned char BLERX_LEN = 0;        /* 当前帧有效字节数（不含 '\0'）                */
// volatile uint32_t Bluetooth_TestTxCount = 0; /* 测试用：累计发送 Pong / Ack 帧数              */
// volatile uint32_t Bluetooth_TestRxCount = 0; /* 测试用：累计接收到有效帧数                    */
// volatile uint32_t Bluetooth_RawRxCount = 0;  /* 诊断用：累计进入 UART RX 中断的原始字节数     */
// volatile uint8_t Bluetooth_LastRxByte = 0;   /* 诊断用：最近一个 UART RX 原始字节              */

// /******************************************************************
//  * 函 数 名 称：Bluetooth_VPrintf
//  * 函 数 说 明：蓝牙格式化发送内部函数
//  * 函 数 形 参：format 格式化字符串
//  *              arg    可变参数列表
//  * 函 数 返 回：无
// ******************************************************************/
// static void Bluetooth_VPrintf(char *format, va_list arg)
// {
//     char String[BLERX_LEN_MAX];

//     vsnprintf(String, sizeof(String), format, arg);
//     Send_Bluetooth_Data(String);
// }

// /******************************************************************
//  * 函 数 名 称：Bluetooth_Printf
//  * 函 数 说 明：蓝牙格式化发送，类似 printf
//  * 函 数 形 参：format 格式化字符串
//  * 函 数 返 回：无
// ******************************************************************/
// void Bluetooth_Printf(char *format, ...)
// {
//     va_list arg;

//     va_start(arg, format);
//     Bluetooth_VPrintf(format, arg);
//     va_end(arg);
// }

// /******************************************************************
//  * 函 数 名 称：BlueSerial_Printf
//  * 函 数 说 明：兼容 STM32 BlueSerial_Printf 命名
//  * 函 数 形 参：format 格式化字符串
//  * 函 数 返 回：无
// ******************************************************************/
// void BlueSerial_Printf(char *format, ...)
// {
//     va_list arg;

//     va_start(arg, format);
//     Bluetooth_VPrintf(format, arg);
//     va_end(arg);
// }

// /******************************************************************
//  * 函 数 名 称：BLE_Send_Bit
//  * 函 数 说 明：向 HC-04 发送单个字符
//  * 函 数 形 参：ch ASCII字符
//  * 函 数 返 回：无
// ******************************************************************/
// void BLE_Send_Bit(unsigned char ch)
// {
//     while(DL_UART_isBusy(UART_0_INST) == true);
//     DL_UART_Main_transmitData(UART_0_INST, ch);
// }

// /******************************************************************
//  * 函 数 名 称：BLE_send_String
//  * 函 数 说 明：向 HC-04 发送字符串
//  * 函 数 形 参：str 发送的字符串
//  * 函 数 返 回：无
// ******************************************************************/
// void BLE_send_String(unsigned char *str)
// {
//     while(str && *str)
//     {
//         BLE_Send_Bit(*str++);
//     }
// }

// /******************************************************************
//  * 函 数 名 称：Clear_BLERX_BUFF
//  * 函 数 说 明：清除串口接收的数据
//  * 函 数 形 参：无
//  * 函 数 返 回：无
// ******************************************************************/
// void Clear_BLERX_BUFF(void)
// {
//     BLERX_LEN = 0;
//     BLERX_BUFF[0] = '\0';
//     BLERX_FLAG = 0;
// }

// /******************************************************************
//  * 函 数 名 称：Bluetooth_Init
//  * 函 数 说 明：HC-04 SLAVE 透明传输初始化
//  * 函 数 形 参：无
//  * 函 数 返 回：无
//  * 备       注：模块正常通信模式为 9600、8N1；这里不发送 AT 指令
// ******************************************************************/
// void Bluetooth_Init(void)
// {
//     Bluetooth_ConnectFlag = 0;
//     Bluetooth_TestTxCount = 0;
//     Bluetooth_TestRxCount = 0;
//     Bluetooth_RawRxCount = 0;
//     Bluetooth_LastRxByte = 0;
//     Clear_BLERX_BUFF();

//     NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
//     NVIC_EnableIRQ(UART_0_INST_INT_IRQN);

// #if DEBUG
//     lc_printf("HC-04 SLAVE Bluetooth_Init succeed!\r\n");
// #endif
// }

// /******************************************************************
//  * 函 数 名 称：Bluetooth_TestTask
//  * 函 数 说 明：HC-04 SLAVE 裸板最小通信测试
//  * 函 数 形 参：tick_10ms 10ms系统节拍，本版本暂不使用
//  * 函 数 返 回：无
//  * 备       注：收到 [MASTER:PING] 回复 [SLAVE:PONG]；收到其他
//  *              普通测试帧回复 [SLAVE:ACK]。
// ******************************************************************/
// void Bluetooth_TestTask(uint32_t tick_10ms)
// {
//     (void)tick_10ms;

//     if(BLERX_FLAG == 1U)
//     {
//         Bluetooth_TestRxCount++;
//         Bluetooth_ConnectFlag = 1U;

//         if(strcmp((char *)BLERX_BUFF, "MASTER:PING") == 0)
//         {
//             Bluetooth_Printf("[%s:PONG]", HC04_DEVICE_NAME);
//             Bluetooth_TestTxCount++;
//         }
//         else if(strcmp((char *)BLERX_BUFF, "MASTER:ACK") != 0)
//         {
//             Bluetooth_Printf("[%s:ACK]", HC04_DEVICE_NAME);
//             Bluetooth_TestTxCount++;
//         }

//         Clear_BLERX_BUFF();
//     }
// }

// /******************************************************************
//  * 函 数 名 称：Get_Bluetooth_ConnectFlag
//  * 函 数 说 明：获取是否收到过对端有效测试帧
//  * 函 数 返 回：1收到过，0尚未收到
// ******************************************************************/
// unsigned char Get_Bluetooth_ConnectFlag(void)
// {
//     return Bluetooth_ConnectFlag;
// }

// /******************************************************************
//  * 函 数 名 称：Bluetooth_Mode
//  * 函 数 说 明：兼容原有接口；当前 STATE 未接 MCU，由测试帧更新状态
//  * 函 数 形 参：无
//  * 函 数 返 回：无
// ******************************************************************/
// void Bluetooth_Mode(void)
// {
//     /* 保留接口，不伪造 STATE 引脚状态。 */
// }

// /******************************************************************
//  * 函 数 名 称：Receive_Bluetooth_Data
//  * 函 数 说 明：通过调试口显示并清除已收到的数据
//  * 函 数 形 参：无
//  * 函 数 返 回：无
// ******************************************************************/
// void Receive_Bluetooth_Data(void)
// {
//     if(BLERX_FLAG == 1U)
//     {
//         lc_printf("data = %s\r\n", BLERX_BUFF);
//         Clear_BLERX_BUFF();
//     }
// }

// /******************************************************************
//  * 函 数 名 称：Send_Bluetooth_Data
//  * 函 数 说 明：向 HC-04 发送字符串
//  * 函 数 形 参：dat 要发送的字符串
//  * 函 数 返 回：无
// ******************************************************************/
// void Send_Bluetooth_Data(char *dat)
// {
//     BLE_send_String((unsigned char *)dat);
// }

// // UART0 中断服务函数：接收 [payload] 格式测试帧
// void UART_0_INST_IRQHandler(void)
// {
//     static unsigned char RxState = 0;
//     static unsigned char pRxPacket = 0;
//     unsigned char RxData;

//     switch(DL_UART_getPendingInterrupt(UART_0_INST))
//     {
//         case DL_UART_IIDX_RX:
//             RxData = DL_UART_Main_receiveData(UART_0_INST);
//             Bluetooth_LastRxByte = RxData;
//             Bluetooth_RawRxCount++;

//             if(RxState == 0)
//             {
//                 if((RxData == '[') && (BLERX_FLAG == 0U))
//                 {
//                     RxState = 1;
//                     pRxPacket = 0;
//                 }
//             }
//             else
//             {
//                 if(RxData == ']')
//                 {
//                     RxState = 0;
//                     BLERX_BUFF[pRxPacket] = '\0';
//                     BLERX_LEN = pRxPacket;
//                     BLERX_FLAG = 1;
//                 }
//                 else if(pRxPacket < (BLERX_LEN_MAX - 1U))
//                 {
//                     BLERX_BUFF[pRxPacket++] = RxData;
//                 }
//                 else
//                 {
//                     RxState = 0;
//                     pRxPacket = 0;
//                     BLERX_LEN = 0;
//                     BLERX_BUFF[0] = '\0';
//                 }
//             }
//             break;

//         default:
//             break;
//     }
// }

// #endif
