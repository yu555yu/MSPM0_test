#ifndef __LCD_INIT_H
#define __LCD_INIT_H

#include "Hardware/Board/board.h"

// 设置横屏或者竖屏显示
// 0或1为竖屏
// 2或3为横屏
#define USE_HORIZONTAL 2 

#if USE_HORIZONTAL==0 || USE_HORIZONTAL==1
    // 竖屏模式：宽170，高320
    #define LCD_W 170
    #define LCD_H 320

#else
    // 横屏模式：宽320，高170 (这里之前写反了，已修正)
    #define LCD_W 320
    #define LCD_H 170
#endif

//-----------------LCD端口定义----------------

#define LCD_RES_Clr()  DL_GPIO_clearPins(LCD_PORT,LCD_RES_PIN)//RES
#define LCD_RES_Set()  DL_GPIO_setPins(LCD_PORT,LCD_RES_PIN)

#define LCD_DC_Clr()   DL_GPIO_clearPins(LCD_PORT,LCD_DC_PIN)//DC
#define LCD_DC_Set()   DL_GPIO_setPins(LCD_PORT,LCD_DC_PIN)

#define LCD_CS_Clr()   DL_GPIO_clearPins(LCD_PORT,LCD_CS_PIN)//CS
#define LCD_CS_Set()   DL_GPIO_setPins(LCD_PORT,LCD_CS_PIN)

#define LCD_BLK_Clr()  DL_GPIO_clearPins(LCD_PORT,LCD_BLK_PIN)//BLK
#define LCD_BLK_Set()  DL_GPIO_setPins(LCD_PORT,LCD_BLK_PIN)

//-----------------函数声明----------------
void LCD_GPIO_Init(void);//初始化GPIO
void LCD_Writ_Bus(u8 dat);//模拟SPI时序
void LCD_WR_DATA8(u8 dat);//写入一个字节
void LCD_WR_DATA(u16 dat);//写入两个字节
void LCD_WR_REG(u8 dat);//写入一个指令
void LCD_Address_Set(u16 x1,u16 y1,u16 x2,u16 y2);//设置坐标函数
void LCD_Init(void);//LCD初始化

#endif