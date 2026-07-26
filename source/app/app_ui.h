#ifndef _APP_UI_H_
#define _APP_UI_H_

#include "ti_msp_dl_config.h"
#include "stdint.h"
#include "Hardware/LCD/lcd.h"
#include "Hardware/Board/board.h"

#define screen_center_x (LCD_W/2)//屏幕中心X = 屏幕x/2
#define screen_center_y (LCD_H/2)//屏幕中心Y = 屏幕y/2

#define SPEED_WAVEFORM_REDUCTION_FACTOR 2.3         // 定速波形衰减倍数
#define DISTANCE_WAVEFORM_REDUCTION_FACTOR 8.3      // 定距波形衰减倍数

void ui_home_page(void);                //绘制首页静态UI
void ui_home_page_select(int mode);     //绘制首页选择框

void ui_speed_page(void);//绘制定速页静态UI
void ui_speed_page_value_set(float p, float i, float d, int speed, int target,int quick_update);     //绘制定速页参数值的变化
void ui_speed_page_select_box(int mode);//绘制定速页选择框

void ui_distance_page(void);//UI绘制定距页静态UI
void ui_distance_page_value_set(float p, float i, float d, int distance, int target,int quick_update);//绘制定距页参数值的变化

void ui_select_page_show(unsigned char select_flag);//根据选择确定显示哪一个页面

void disp_x_center(int y, uint16_t bc, unsigned char sizey, const char* str);//绘制彩色填充矩形，带居中的字符串
void disp_string_rect(int x, int w, int y, int h, int sizey, const char* str, int color);//绘制彩色填充矩形，带居中的字符串
void disp_select_box(int x, int w, int y, int h, int line_length, int interval, int color);//功能：绘制选择框
void disp_string_center_in_rect(int x, int y, int w, int h, const char* str, uint16_t fc, uint16_t bc, unsigned char sizey);//在指定矩形区域内居中显示字符串，不重绘背景矩形
void LCD_DrawRectangle_Bold(u16 x1, u16 y1, u16 x2, u16 y2, u8 bold, u16 color);//绘制选中实心框,后加
void ui_parameter_select_box_bold(int mode);//绘制参数选中框

uint16_t draw_distance_curve(int window_start_x,int window_start_y,int window_w,int window_h,int curve_color,int background_color,short int rawValue);
uint16_t draw_speed_curve(int window_start_x,int window_start_y,int window_w,int window_h,int curve_color,int background_color,short int rawValue);
void ui_speed_curve(void);
void ui_distance_curve(void);

void disp_int_value_box_init(int x, int w, int y, int h, int sizey,const char* label, int32_t value, int digits,uint16_t fc, uint16_t bc);
void disp_int_value_box_update(int x, int w, int y, int h, int sizey,const char* label, int32_t value, int digits,uint16_t fc, uint16_t bc);
void disp_float_value_box_init(int x, int w, int y, int h, int sizey,const char* label, float value, int digits,uint16_t fc, uint16_t bc);
void disp_float_value_box_update(int x, int w, int y, int h, int sizey,const char* label, float value, int digits,uint16_t fc, uint16_t bc);
#endif
