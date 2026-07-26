#include "lcd_init.h"
//#include "delay.h"

void LCD_GPIO_Init(void)
{
    // 这里应该填入具体的GPIO初始化代码
    // 例如：DL_GPIO_initPeripheralOutputFunction(...)
}

/******************************************************************************
      函数说明：LCD串行数据写入函数
      入口数据：dat  要写入的串行数据
      返回值：  无
******************************************************************************/
void LCD_Writ_Bus(u8 dat) 
{	
    LCD_CS_Clr();

    DL_SPI_transmitDataBlocking8(SPI_LCD_INST, dat);
    while(DL_SPI_isBusy(SPI_LCD_INST));

    LCD_CS_Set();
}


/******************************************************************************
      函数说明：LCD写入数据
      入口数据：dat 写入的数据
      返回值：  无
******************************************************************************/
void LCD_WR_DATA8(u8 dat)
{
	LCD_Writ_Bus(dat);
}


/******************************************************************************
      函数说明：LCD写入数据
      入口数据：dat 写入的数据
      返回值：  无
******************************************************************************/
void LCD_WR_DATA(u16 dat)
{
	LCD_Writ_Bus(dat>>8);
	LCD_Writ_Bus(dat);
}


/******************************************************************************
      函数说明：LCD写入命令
      入口数据：dat 写入的命令
      返回值：  无
******************************************************************************/
void LCD_WR_REG(u8 dat)
{
	LCD_DC_Clr();//写命令
	LCD_Writ_Bus(dat);
	LCD_DC_Set();//写数据
}

/******************************************************************************
      函数说明：LCD初始化
      入口数据：无
      返回值：  无
******************************************************************************/
void LCD_Init(void)
{
	LCD_GPIO_Init();//初始化GPIO

	LCD_RES_Clr();//复位
	delay_ms(100);
	LCD_RES_Set();
	delay_ms(100);

	LCD_BLK_Set();//打开背光
    delay_ms(100);

	//************* Start Initial Sequence **********//
	LCD_WR_REG(0x11); //Sleep out
	delay_ms(120);              //Delay 120ms
	//************* Start Initial Sequence **********//
	LCD_WR_REG(0x36);
	if(USE_HORIZONTAL==0)LCD_WR_DATA8(0x00);
	else if(USE_HORIZONTAL==1)LCD_WR_DATA8(0xC0);
	else if(USE_HORIZONTAL==2)LCD_WR_DATA8(0x70);
	else LCD_WR_DATA8(0xA0);

	LCD_WR_REG(0x3A);
	LCD_WR_DATA8(0x05);

	LCD_WR_REG(0xB2);
	LCD_WR_DATA8(0x0C);
	LCD_WR_DATA8(0x0C);
	LCD_WR_DATA8(0x00);
	LCD_WR_DATA8(0x33);
	LCD_WR_DATA8(0x33);

	LCD_WR_REG(0xB7);
	LCD_WR_DATA8(0x35);

	LCD_WR_REG(0xBB);
	LCD_WR_DATA8(0x32); //Vcom=1.35V

	LCD_WR_REG(0xC2);
	LCD_WR_DATA8(0x01);

	LCD_WR_REG(0xC3);
	LCD_WR_DATA8(0x15); //GVDD=4.8V

	LCD_WR_REG(0xC4);
	LCD_WR_DATA8(0x20); //VDV, 0x20:0v

	LCD_WR_REG(0xC6);
	LCD_WR_DATA8(0x0F); //0x0F:60Hz

	LCD_WR_REG(0xD0);
	LCD_WR_DATA8(0xA4);
	LCD_WR_DATA8(0xA1);

	LCD_WR_REG(0xE0);
	LCD_WR_DATA8(0xD0);
	LCD_WR_DATA8(0x08);
	LCD_WR_DATA8(0x0E);
	LCD_WR_DATA8(0x09);
	LCD_WR_DATA8(0x09);
	LCD_WR_DATA8(0x05);
	LCD_WR_DATA8(0x31);
	LCD_WR_DATA8(0x33);
	LCD_WR_DATA8(0x48);
	LCD_WR_DATA8(0x17);
	LCD_WR_DATA8(0x14);
	LCD_WR_DATA8(0x15);
	LCD_WR_DATA8(0x31);
	LCD_WR_DATA8(0x34);

	LCD_WR_REG(0xE1);
	LCD_WR_DATA8(0xD0);
	LCD_WR_DATA8(0x08);
	LCD_WR_DATA8(0x0E);
	LCD_WR_DATA8(0x09);
	LCD_WR_DATA8(0x09);
	LCD_WR_DATA8(0x15);
	LCD_WR_DATA8(0x31);
	LCD_WR_DATA8(0x33);
	LCD_WR_DATA8(0x48);
	LCD_WR_DATA8(0x17);
	LCD_WR_DATA8(0x14);
	LCD_WR_DATA8(0x15);
	LCD_WR_DATA8(0x31);
	LCD_WR_DATA8(0x34);

	LCD_WR_REG(0x21); // 颜色反转

	LCD_WR_REG(0x29); // 开启显示

    // --- 关键修改：初始化完成后，强制设置一次全屏窗口 ---
    LCD_Address_Set(0, 0, LCD_W-1, LCD_H-1);
}

// // ==========================================
// // 适配 1.9寸屏 (170x320) 的窗口设置函数
// // 解决了 X 轴偏移 35 的问题
// // ==========================================
// void LCD_Address_Set(u16 x0, u16 y0, u16 x1, u16 y1)
// {
//     // 1.9寸屏幕分辨率为 170x320
//     // ST7789 驱动芯片通常默认管理 240x320 的区域
//     // X 轴偏移量 = (240 - 170) / 2 = 35

//     if(USE_HORIZONTAL == 0) // 竖屏模式 (170x320)
//     {
//         LCD_WR_REG(0x2A); // 列地址设置 (X轴)
//         LCD_WR_DATA8(0x00); LCD_WR_DATA8(x0 + 35); // 起始X + 35偏移
//         LCD_WR_DATA8(0x00); LCD_WR_DATA8(x1 + 35); // 结束X + 35偏移

//         LCD_WR_REG(0x2B); // 行地址设置 (Y轴)
//         LCD_WR_DATA8(0x00); LCD_WR_DATA8(y0);      // 起始Y
//         LCD_WR_DATA8(0x01); LCD_WR_DATA8(y1);      // 结束Y (注意：如果 y1>255，需处理高位)
//     }
//     else if(USE_HORIZONTAL == 1) // 竖屏翻转
//     {
//         LCD_WR_REG(0x2A);
//         LCD_WR_DATA8(0x00); LCD_WR_DATA8(x0 + 35);
//         LCD_WR_DATA8(0x00); LCD_WR_DATA8(x1 + 35);

//         LCD_WR_REG(0x2B);
//         LCD_WR_DATA8(0x00); LCD_WR_DATA8(y0);
//         LCD_WR_DATA8(0x01); LCD_WR_DATA8(y1);
//     }
//     else if(USE_HORIZONTAL == 2) // 横屏模式 (320x170)
//     {
//         // 横屏时，原来的 Y 轴变成了 X 轴（不需要偏移）
//         // 原来的 X 轴变成了 Y 轴（需要偏移）
        
//         LCD_WR_REG(0x2A); // 设置 X轴
//         LCD_WR_DATA8(0x00); LCD_WR_DATA8(x0);
//         LCD_WR_DATA8(0x01); LCD_WR_DATA8(x1);

//         LCD_WR_REG(0x2B); // 设置 Y轴
//         LCD_WR_DATA8(0x00); LCD_WR_DATA8(y0 + 35); // Y轴加偏移
//         LCD_WR_DATA8(0x00); LCD_WR_DATA8(y1 + 35);
//     }
//     else // 横屏翻转
//     {
//         LCD_WR_REG(0x2A);
//         LCD_WR_DATA8(0x00); LCD_WR_DATA8(x0);
//         LCD_WR_DATA8(0x01); LCD_WR_DATA8(x1);

//         LCD_WR_REG(0x2B);
//         LCD_WR_DATA8(0x00); LCD_WR_DATA8(y0 + 35);
//         LCD_WR_DATA8(0x00); LCD_WR_DATA8(y1 + 35);
//     }

//     LCD_WR_REG(0x2C); // 开始写入GRAM
// }
void LCD_Address_Set(u16 x0, u16 y0, u16 x1, u16 y1)
{
#if USE_HORIZONTAL == 0
    // 竖屏：170x320
    // X方向加35偏移
    x0 += 35;
    x1 += 35;

#elif USE_HORIZONTAL == 1
    // 竖屏翻转：170x320
    // X方向加35偏移
    x0 += 35;
    x1 += 35;

#elif USE_HORIZONTAL == 2
    // 横屏：320x170
    // Y方向加35偏移
    y0 += 35;
    y1 += 35;

#elif USE_HORIZONTAL == 3
    // 横屏翻转：320x170
    // Y方向加35偏移
    y0 += 35;
    y1 += 35;
#endif

    LCD_WR_REG(0x2A);
    LCD_WR_DATA8(x0 >> 8);
    LCD_WR_DATA8(x0 & 0xFF);
    LCD_WR_DATA8(x1 >> 8);
    LCD_WR_DATA8(x1 & 0xFF);

    LCD_WR_REG(0x2B);
    LCD_WR_DATA8(y0 >> 8);
    LCD_WR_DATA8(y0 & 0xFF);
    LCD_WR_DATA8(y1 >> 8);
    LCD_WR_DATA8(y1 & 0xFF);

    LCD_WR_REG(0x2C);
}
