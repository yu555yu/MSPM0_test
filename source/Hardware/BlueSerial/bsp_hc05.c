#include "bsp_hc05.h"
#include "stdio.h"
#include <stdarg.h>

unsigned char Bluetooth_ConnectFlag = 0; // 蓝牙连接状态 = 0没有手机连接   = 1有手机连接
unsigned char BLERX_BUFF[BLERX_LEN_MAX];
volatile unsigned char BLERX_FLAG = 0;
volatile unsigned char BLERX_LEN = 0;

/******************************************************************
 * 函 数 名 称：Bluetooth_VPrintf
 * 函 数 说 明：蓝牙格式化发送内部函数
 * 函 数 形 参：format 格式化字符串
 *              arg    可变参数列表
 * 函 数 返 回：无
******************************************************************/
static void Bluetooth_VPrintf(char *format, va_list arg)
{
    char String[100];

    vsnprintf(String, sizeof(String), format, arg);

    Send_Bluetooth_Data(String);
}


/******************************************************************
 * 函 数 名 称：Bluetooth_Printf
 * 函 数 说 明：蓝牙格式化发送，类似 printf
 * 函 数 形 参：format 格式化字符串
 * 函 数 返 回：无
******************************************************************/
void Bluetooth_Printf(char *format, ...)
{
    va_list arg;

    va_start(arg, format);

    Bluetooth_VPrintf(format, arg);

    va_end(arg);
}


/******************************************************************
 * 函 数 名 称：BlueSerial_Printf
 * 函 数 说 明：兼容 STM32 BlueSerial_Printf 命名
 * 函 数 形 参：format 格式化字符串
 * 函 数 返 回：无
******************************************************************/
void BlueSerial_Printf(char *format, ...)
{
    va_list arg;

    va_start(arg, format);

    Bluetooth_VPrintf(format, arg);

    va_end(arg);
}

/******************************************************************
 * 函 数 名 称：BLE_Send_Bit
 * 函 数 说 明：向蓝牙发送单个字符
 * 函 数 形 参：ch=ASCII字符
 * 函 数 返 回：无
 * 作       者：LC
 * 备       注：无
******************************************************************/
void BLE_Send_Bit(unsigned char ch)
{
    //当串口0忙的时候等待，不忙的时候再发送传进来的字符
    while( DL_UART_isBusy(UART_0_INST) == true );
    //发送单个字符
    DL_UART_Main_transmitData(UART_0_INST, ch);
}

/******************************************************************
 * 函 数 名 称：BLE_send_String
 * 函 数 说 明：向蓝牙发送字符串
 * 函 数 形 参：str=发送的字符串
 * 函 数 返 回：无
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
 * 函 数 名 称：Clear_BLERX_BUFF
 * 函 数 说 明：清除串口接收的数据
 * 函 数 形 参：无
 * 函 数 返 回：无
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
 * 函 数 名 称：Bluetooth_Init
 * 函 数 说 明：蓝牙初始化
 * 函 数 形 参：无
 * 函 数 返 回：无
 * 作       者：LC
 * 备       注：默认波特率为9600
******************************************************************/
void Bluetooth_Init(void)
{
    //清除串口中断标志
    NVIC_ClearPendingIRQ(UART_0_INST_INT_IRQN);
    //使能串口中断
    NVIC_EnableIRQ(UART_0_INST_INT_IRQN);

    #if DEBUG
         //在调试时，通过AT命令已经设置好模式
        lc_printf("Bluetooth_Init succeed!\r\n");

    #endif
}

/******************************************************************
 * 函 数 名 称：Get_Bluetooth_ConnectFlag
 * 函 数 说 明：获取手机连接状态
 * 函 数 形 参：无
 * 函 数 返 回：返回1=已连接                返回0=未连接
 * 作       者：LC
 * 备       注：使用该函数前，必须先调用 Bluetooth_Mode 函数
******************************************************************/
unsigned char Get_Bluetooth_ConnectFlag(void)
{
    return Bluetooth_ConnectFlag;
}

/******************************************************************
 * 函 数 名 称：Bluetooth_Mode
 * 函 数 说 明：判断蓝牙模块的连接状态
 * 函 数 形 参：无
 * 函 数 返 回：无
 * 作       者：LC
 * 备       注：未连接时STATE低电平   连接成功时STATE高电平
******************************************************************/
void Bluetooth_Mode(void)
{
    Bluetooth_ConnectFlag = 1;  //STATE 悬空，默认连接
}

/******************************************************************
 * 函 数 名 称：Receive_Bluetooth_Data
 * 函 数 说 明：接收蓝牙数据
 * 函 数 形 参：无
 * 函 数 返 回：无
 * 作       者：LC
 * 备       注：无
******************************************************************/
void Receive_Bluetooth_Data(void)
{
    if( BLERX_FLAG == 1 )//接收到蓝牙数据
    {
        //显示蓝牙发送过来的数据
        lc_printf("data = %s\r\n",BLERX_BUFF);
        Clear_BLERX_BUFF();//清除接收缓存
    }
}

/******************************************************************
 * 函 数 名 称：Send_Bluetooth_Data
 * 函 数 说 明：向蓝牙模块发送数据
 * 函 数 形 参：dat=要发送的字符串
 * 函 数 返 回：无
 * 作       者：LC
 * 备       注：（如果手机连接了蓝牙，就是向手机发送数据）
******************************************************************/
void Send_Bluetooth_Data(char *dat)
{
    BLE_send_String((unsigned char*)dat);//不用判断 STATE，直接发
}

//串口的中断服务函数
void UART_0_INST_IRQHandler(void)
{
    static unsigned char RxState = 0;
    static unsigned char pRxPacket = 0;
    unsigned char RxData;

    switch(DL_UART_getPendingInterrupt(UART_0_INST))
    {
        case DL_UART_IIDX_RX:

            RxData = DL_UART_Main_receiveData(UART_0_INST);

            /*
                RxState = 0：等待起始符 [
                RxState = 1：正在接收数据，直到遇到 ]
            */
            if(RxState == 0)
            {
                if(RxData == '[' && BLERX_FLAG == 0)
                {
                    RxState = 1;
                    pRxPacket = 0;
                }
            }
            else if(RxState == 1)
            {
                if(RxData == ']')
                {
                    RxState = 0;
                    BLERX_BUFF[pRxPacket] = '\0';
                    BLERX_LEN = pRxPacket;
                    BLERX_FLAG = 1;
                }
                else
                {
                    if(pRxPacket < BLERX_LEN_MAX - 1)
                    {
                        BLERX_BUFF[pRxPacket++] = RxData;
                    }
                    else
                    {
                        /*
                            数据太长，丢弃本包
                        */
                        RxState = 0;
                        pRxPacket = 0;
                        BLERX_LEN = 0;
                        BLERX_BUFF[0] = '\0';
                    }
                }
            }

            break;

        default:
            break;
    }
}