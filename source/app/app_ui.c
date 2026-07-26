#include "app_ui.h"
#include "string.h"
#include "stdio.h"
#include "Hardware/LCD/lcd.h"
#include "Hardware/LCD/lcd_init.h"
#include "Hardware/Encoder/Encoder.h"

#define UI_BOX_PAD_X        4
#define UI_BOX_VALUE_GAP    2
#define UI_BOX_MIN_VALUE_CH 1
#define UI_BOX_TEXT_BUF_LEN 24

typedef struct
{
    int label_x;
    int text_y;
    int value_x;
    int value_w;
} UiValueBoxLayout;

static UiValueBoxLayout ui_get_value_box_layout(int x, int w, int y, int h,
    int sizey, const char *label, int reserve_chars)
{
    UiValueBoxLayout layout;
    int char_w = sizey / 2;
    int label_w = (int)strlen(label) * char_w;
    int min_value_chars = reserve_chars;
    int value_w;
    int value_right;
    int min_value_x;

    if (min_value_chars < UI_BOX_MIN_VALUE_CH)
    {
        min_value_chars = UI_BOX_MIN_VALUE_CH;
    }

    layout.label_x = x + UI_BOX_PAD_X;
    layout.text_y = y + (h - sizey) / 2;
    value_w = min_value_chars * char_w;
    value_right = x + w - UI_BOX_PAD_X;
    layout.value_x = value_right - value_w;
    min_value_x = layout.label_x + label_w + UI_BOX_VALUE_GAP;
    if (layout.value_x < min_value_x)
    {
        layout.value_x = min_value_x;
    }

    layout.value_w = value_right - layout.value_x;
    if (layout.value_w < char_w)
    {
        layout.value_w = char_w;
        layout.value_x = value_right - layout.value_w;
    }

    return layout;
}

static void ui_draw_value_text(int x, int w, int y, int h, int sizey,
    const char *label, const char *text, int reserve_chars,
    uint16_t fc, uint16_t bc)
{
    int char_w = sizey / 2;
    int text_len = (int)strlen(text);
    UiValueBoxLayout layout = ui_get_value_box_layout(x, w, y, h, sizey,
        label, text_len > reserve_chars ? text_len : reserve_chars);
    int slot_chars = layout.value_w / char_w;
    char disp[UI_BOX_TEXT_BUF_LEN];
    int pad_chars;

    if (slot_chars < UI_BOX_MIN_VALUE_CH)
    {
        slot_chars = UI_BOX_MIN_VALUE_CH;
    }

    if (slot_chars >= (int)sizeof(disp))
    {
        slot_chars = (int)sizeof(disp) - 1;
    }

    if (text_len >= slot_chars)
    {
        LCD_ShowString(layout.value_x, layout.text_y, text, fc, bc, sizey, 0);
        return;
    }

    memset(disp, ' ', (size_t)slot_chars);
    pad_chars = slot_chars - text_len;
    memcpy(&disp[pad_chars], text, (size_t)text_len);
    disp[slot_chars] = '\0';

    LCD_ShowString(layout.value_x, layout.text_y, disp, fc, bc, sizey, 0);
}

/*
    功能：基于屏幕X轴中心居中绘制彩色填充矩形，带居中的字符串
    参数：
            y=矩形起始Y轴
            str_len=中文字符个数
            bc=矩形背景色
            sizey=字符像素大小
            *str=要显示的字符串
*/
void disp_x_center(int y, uint16_t bc, unsigned char sizey, const char* str)
{
    int str_len = strlen(str);
    int str_center_x = (sizey/2 * str_len) / 2;//字符中心x=字符串像素大小*字符串字符个数/2

    //绘制标题的圆角矩形
    LCD_ArcRect(screen_center_x - str_center_x - 10, y, screen_center_x + str_center_x + 10, sizey+y, bc);
    LCD_ShowString(screen_center_x - str_center_x,y,str,WHITE,bc,sizey,0);
}

/*
    功能：绘制彩色填充矩形，带居中的字符串
    参数：  x=矩形起始X轴
            w=矩形宽度
            y=矩形起始Y轴
            h=矩形高度
            str_len=字符个数
            sizey=字符像素大小
            *str=要显示的字符串
            color矩形背景色
    备注：GRAYBLUE 浅蓝
        DARKBLUE 深蓝
*/
void disp_string_rect(int x, int w, int y, int h, int sizey, const char* str, int color)
{
    int str_len = strlen(str);
    int str_width = str_len * (sizey / 2);      // 英文字符串宽度 = 字符个数 * 单字符宽度
    int str_center_x = str_width / 2;           // 字符串中心x
    int rect_center_x = x + (w / 2);            // 矩形中心x
    int str_center_y = sizey / 2;               // 字符中心y
    int rect_center_y = y + (h / 2);            // 矩形中心y
    // 绘制背景矩形
    LCD_ArcRect(x, y, x + w - 1, y + h - 1, color);
    // 绘制字符串
    LCD_ShowString(rect_center_x - str_center_x,rect_center_y - str_center_y, str,WHITE,color,sizey,0);
}

/*
    功能：在指定矩形区域内居中显示字符串，不重绘背景矩形
    参数：
            x=矩形起始X轴
            y=矩形起始Y轴
            w=矩形宽度
            h=矩形高度
            str=要显示的字符串
            fc=字体颜色
            bc=字体背景色
            sizey=字符像素大小
*/
void disp_string_center_in_rect(int x, int y, int w, int h, const char* str, uint16_t fc, uint16_t bc, unsigned char sizey)
{
    int str_len = strlen(str);
    int str_width = str_len * (sizey / 2);

    int show_x = x + (w - str_width) / 2;
    int show_y = y + (h - sizey) / 2;

    LCD_ShowString(show_x, show_y, str, fc, bc, sizey, 0);
}

/*
    功能：绘制选择框
    参数：  x=起始X轴地址
            w=绘制的选择框矩形宽度
            y=起始Y轴地址
            h=绘制的选择框矩形高度
            line_length=选择框的线长度
            interval=选择框 与 被选择矩形 之间的间隔像素
            color=选择框的颜色
*/
void disp_select_box(int x, int w, int y, int h, int line_length, int interval, int color)
{
    //计算 选择框 与 被选择矩形 的距离间隔
    x = x - interval;
    w = w + (interval + interval);
    y = y - interval;
    h = h + (interval + interval);
    //左上角
    LCD_DrawLine(x, y, x + line_length, y, color);
    LCD_DrawLine(x, y, x, y + line_length, color);
    //右上角
    LCD_DrawLine(x + w, y, x + w - line_length, y, color);
    LCD_DrawLine(x + w, y, x + w, y + line_length, color);
    //左下角
    LCD_DrawLine(x, y + h, x + line_length, y + h, color);
    LCD_DrawLine(x, y + h, x, y + h - line_length, color);
    //右下角
    LCD_DrawLine(x + w, y + h, x + w - line_length, y + h, color);
    LCD_DrawLine(x + w, y + h, x + w, y + h - line_length, color);
}

/*
    功能：初始化 label + 整数数字显示框
    特点：只画一次背景框和 label
*/
void disp_int_value_box_init(int x, int w, int y, int h, int sizey,const char* label, int32_t value, int digits,uint16_t fc, uint16_t bc)
{
    char buff[20];
    UiValueBoxLayout layout;

    snprintf(buff, sizeof(buff), "%ld", (long)value);
    layout = ui_get_value_box_layout(x, w, y, h, sizey, label,
        (int)strlen(buff) > digits ? (int)strlen(buff) : digits);
    LCD_ArcRect(x, y, x + w - 1, y + h - 1, bc);
    LCD_ShowString(layout.label_x, layout.text_y, label, fc, bc, sizey, 0);
    ui_draw_value_text(x, w, y, h, sizey, label, buff, digits, fc, bc);
}



/*
    功能：只更新 label + 整数显示框中的数字
    特点：不重画框，不重画 label
*/
void disp_int_value_box_update(int x, int w, int y, int h, int sizey,const char* label, int32_t value, int digits,uint16_t fc, uint16_t bc)
{
    char buff[20];

    snprintf(buff, sizeof(buff), "%ld", (long)value);
    ui_draw_value_text(x, w, y, h, sizey, label, buff, digits, fc, bc);
}

/*
    功能：初始化 label + 浮点数字显示框
    特点：只画一次背景框和 label
    备注：digits 表示显示区域字符宽度，不是小数位数
*/
void disp_float_value_box_init(int x, int w, int y, int h, int sizey,const char* label, float value, int digits,uint16_t fc, uint16_t bc)
{
    char buff[20];
    UiValueBoxLayout layout;

    snprintf(buff, sizeof(buff), "%.2f", value);
    layout = ui_get_value_box_layout(x, w, y, h, sizey, label,
        (int)strlen(buff) > digits ? (int)strlen(buff) : digits);
    LCD_ArcRect(x, y, x + w - 1, y + h - 1, bc);
    LCD_ShowString(layout.label_x, layout.text_y, label, fc, bc, sizey, 0);
    ui_draw_value_text(x, w, y, h, sizey, label, buff, digits, fc, bc);
}

/*
    功能：只更新 label + 浮点显示框中的数字
    特点：不重画框，不重画 label
    备注：digits 表示显示区域字符宽度，用于清除旧数字残留
*/
void disp_float_value_box_update(int x, int w, int y, int h, int sizey,const char* label, float value, int digits,uint16_t fc, uint16_t bc)
{
    char buff[20];

    snprintf(buff, sizeof(buff), "%.2f", value);
    ui_draw_value_text(x, w, y, h, sizey, label, buff, digits, fc, bc);
}
// //绘制首页界面
// void ui_home_page(void)
// {
//     //关闭背光
//     LCD_BLK_Clr();

//     //绘制全局背景
//     LCD_Fill(0,0,LCD_W -1,LCD_H -1,BLACK);
//     //绘制来源
//     disp_x_center(3, BLUE, 16, "York");
//     //绘制标题
//     disp_x_center(22, BLUE, 16, "PID");

//     int btn_w = 80;
//     int btn_h = 80;
//     int btn_y = 65;
//     int gap = 80;

//     int total_w = btn_w * 2 + gap;
//     int speed_x = (LCD_W - total_w) / 2;
//     int dist_x = speed_x + btn_w + gap;

//     //绘制任务一：PID定速
//     disp_string_rect(speed_x, btn_w, btn_y, btn_h, 24, "Speed", BLUE);
//     //绘制任务二：PID定距
//     disp_string_rect(dist_x, btn_w, btn_y, btn_h, 24, "Dist", BLUE);

//      //根据首页当前选择内容 绘制选择框
//     switch( get_default_page_flag() )
//     {
//         case 0:
//         disp_select_box(speed_x, btn_w, btn_y, btn_h, 10, 5, WHITE);
//         break;

//         case 1:
//         disp_select_box(dist_x, btn_w, btn_y, btn_h, 10, 5, WHITE);
//         break;
//     }

// 	LCD_BLK_Set();//打开背光
// }

// //根据按键选择绘制首页两个选项的选择框
// void ui_home_page_select(int mode)
// {
//     char select_box_seze = 5;
//     switch(mode)
//     {
//         case 0:      //选择PID的定速模式
//             disp_select_box(40,80,65,80,10,select_box_seze,WHITE);
//             disp_select_box(200,80,65,80,10,select_box_seze,BLACK); //消隐 
//             break;
//         case 1:      //选择PID的定距模式
//             disp_select_box(40,80,65,80,10,select_box_seze,BLACK);
//             disp_select_box(200,80,65,80,10,select_box_seze,WHITE);
//             break;
//     }
// }

// typedef struct {
//     unsigned int start_x;
//     unsigned int start_y;
//     unsigned int end_x;
//     unsigned int end_y;
//     unsigned int center_x;
//     unsigned int center_y;
// } TXT_OBJECT;

// //绘制定速页界面
// void ui_speed_page(void)
// {
//     TXT_OBJECT p = {0};
//     TXT_OBJECT i = {0};
//     TXT_OBJECT d = {0};

//     // 关闭背光
//     LCD_BLK_Clr();

//     // 绘制全局背景
//     LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, BLACK);

//     // ==============================
//     // PID 参数区域布局
//     // ==============================
//     int pid_offset_y = 32;   // 整体下移像素，想更低就调大

//     int item_w = 70;
//     int item_h = 28;
//     int item_y = 72 + pid_offset_y;

//     int title_y = 50 + pid_offset_y;
//     int font_size = 16;
//     int char_w = font_size / 2;

//     int gap = 14;

//     int total_w = item_w * 3 + gap * 2;
//     int p_x = (LCD_W - total_w) / 2;
//     int i_x = p_x + item_w + gap;
//     int d_x = i_x + item_w + gap;

//     // P 参数框
//     p.start_x = p_x;
//     p.start_y = item_y;
//     p.end_x = p.start_x + item_w - 1;
//     p.end_y = p.start_y + item_h - 1;
//     p.center_x = p.start_x + item_w / 2;
//     p.center_y = p.start_y + item_h / 2;

//     // I 参数框
//     i.start_x = i_x;
//     i.start_y = item_y;
//     i.end_x = i.start_x + item_w - 1;
//     i.end_y = i.start_y + item_h - 1;
//     i.center_x = i.start_x + item_w / 2;
//     i.center_y = i.start_y + item_h / 2;

//     // D 参数框
//     d.start_x = d_x;
//     d.start_y = item_y;
//     d.end_x = d.start_x + item_w - 1;
//     d.end_y = d.start_y + item_h - 1;
//     d.center_x = d.start_x + item_w / 2;
//     d.center_y = d.start_y + item_h / 2;

//     // ==============================
//     // 显示 P I D 标题
//     // ==============================
//     LCD_ShowChar(p.center_x - char_w / 2, title_y, 'P', WHITE, BLACK, font_size, 0);
//     LCD_ShowChar(i.center_x - char_w / 2, title_y, 'I', WHITE, BLACK, font_size, 0);
//     LCD_ShowChar(d.center_x - char_w / 2, title_y, 'D', WHITE, BLACK, font_size, 0);

//     // ==============================
//     // 绘制 P I D 参数背景框
//     // ==============================
//     LCD_ArcRect(p.start_x, p.start_y, p.end_x, p.end_y, BLUE);
//     LCD_ArcRect(i.start_x, i.start_y, i.end_x, i.end_y, BLUE);
//     LCD_ArcRect(d.start_x, d.start_y, d.end_x, d.end_y, BLUE);

//     // ==============================
//     // 参数默认值显示
//     // 后续可以替换为实际 PID 参数
//     // ==============================
//     disp_string_center_in_rect(p.start_x, p.start_y, item_w, item_h, "0.00", WHITE, BLUE, 16);
//     disp_string_center_in_rect(i.start_x, i.start_y, item_w, item_h, "0.00", WHITE, BLUE, 16);
//     disp_string_center_in_rect(d.start_x, d.start_y, item_w, item_h, "0.00", WHITE, BLUE, 16);


//     // ==============================
//     // 底部速度与目标值显示
//     // ==============================
//     LCD_ShowString(20,        136, "Speed:",  WHITE, BLACK, 24, 0);
//     LCD_ShowString(LCD_W-150, 136, "Target:", WHITE, BLACK, 24, 0);

//     // 默认数值
//     LCD_ShowString(98,        136, "0", WHITE, BLACK, 24, 0);
//     LCD_ShowString(LCD_W-54,  136, "0", WHITE, BLACK, 24, 0);

//     // 打开背光
//     LCD_BLK_Set();
// }

// void ui_speed_page_value_set(float p, float i, float d, int speed, int target, int quick_update)
// {
//     static int last_p_show;
//     static int last_i_show;
//     static int last_d_show;
//     static int last_speed;
//     static int last_target;

//     char show_buff[50] = {0};

//     int font_size = 16;
//     int char_width_pixel = font_size / 2;

//     // ==============================
//     // 必须和 ui_speed_page() 中的布局保持一致
//     // ==============================
//     int pid_offset_y = 32;       //这里记得要改

//     int item_w = 70;
//     int item_h = 28;
//     int item_y = 72 + pid_offset_y;

//     int gap = 14;

//     int total_w = item_w * 3 + gap * 2;
//     int p_x = (LCD_W - total_w) / 2;
//     int i_x = p_x + item_w + gap;
//     int d_x = i_x + item_w + gap;

//     // 底部数值显示坐标
//     int speed_value_x = 98;
//     int speed_value_y = 136;

//     int target_value_x = LCD_W - 54;
//     int target_value_y = 136;

//     // 按两位小数缓存，避免浮点误差造成频繁刷新
//     int p_show = (int)(p * 100.0f + (p >= 0 ? 0.5f : -0.5f));
//     int i_show = (int)(i * 100.0f + (i >= 0 ? 0.5f : -0.5f));
//     int d_show = (int)(d * 100.0f + (d >= 0 ? 0.5f : -0.5f));

//     if(quick_update != 1)
//     {
//         last_p_show = 0x7FFFFFFF;
//         last_i_show = 0x7FFFFFFF;
//         last_d_show = 0x7FFFFFFF;
//         last_speed = 0x7FFFFFFF;
//         last_target = 0x7FFFFFFF;
//     }

//     // ==============================
//     // 更新 P
//     // ==============================
//     if(last_p_show != p_show)
//     {
//         last_p_show = p_show;

//         sprintf(show_buff, "%.2f", p);

//         int txt_size = strlen(show_buff) * char_width_pixel;
//         int txt_x = p_x + ((item_w - txt_size) / 2);
//         int txt_y = item_y + ((item_h - font_size) / 2);

//         // 清空参数框内部，防止旧数字残留
//         LCD_Fill(p_x + 4, item_y + 4, p_x + item_w - 5, item_y + item_h - 5, BLUE);

//         LCD_ShowString(txt_x, txt_y, show_buff, WHITE, BLUE, 16, 0);
//     }

//     // ==============================
//     // 更新 I
//     // ==============================
//     if(last_i_show != i_show)
//     {
//         last_i_show = i_show;

//         sprintf(show_buff, "%.2f", i);

//         int txt_size = strlen(show_buff) * char_width_pixel;
//         int txt_x = i_x + ((item_w - txt_size) / 2);
//         int txt_y = item_y + ((item_h - font_size) / 2);

//         LCD_Fill(i_x + 4, item_y + 4, i_x + item_w - 5, item_y + item_h - 5, BLUE);

//         LCD_ShowString(txt_x, txt_y, show_buff, WHITE, BLUE, 16, 0);
//     }

//     // ==============================
//     // 更新 D
//     // ==============================
//     if(last_d_show != d_show)
//     {
//         last_d_show = d_show;

//         sprintf(show_buff, "%.2f", d);

//         int txt_size = strlen(show_buff) * char_width_pixel;
//         int txt_x = d_x + ((item_w - txt_size) / 2);
//         int txt_y = item_y + ((item_h - font_size) / 2);

//         LCD_Fill(d_x + 4, item_y + 4, d_x + item_w - 5, item_y + item_h - 5, BLUE);

//         LCD_ShowString(txt_x, txt_y, show_buff, WHITE, BLUE, 16, 0);
//     }

//     // ==============================
//     // 更新 Speed
//     // ==============================
//     if(last_speed != speed)
//     {
//         last_speed = speed;

//         // 清空速度数值区域
//         LCD_Fill(speed_value_x, speed_value_y, speed_value_x + 55, speed_value_y + 23, BLACK);

//         sprintf(show_buff, "%d", speed);
//         LCD_ShowString(speed_value_x, speed_value_y, show_buff, WHITE, BLACK, 24, 0);
//     }

//     // ==============================
//     // 更新 Target
//     // ==============================
//     if(last_target != target)
//     {
//         last_target = target;

//         // 清空目标数值区域
//         LCD_Fill(target_value_x, target_value_y, target_value_x + 55, target_value_y + 23, BLACK);

//         sprintf(show_buff, "%d", target);
//         LCD_ShowString(target_value_x, target_value_y, show_buff, WHITE, BLACK, 24, 0);
//     }
// }

// //绘制选中实心框
// void LCD_DrawRectangle_Bold(u16 x1, u16 y1, u16 x2, u16 y2, u8 bold, u16 color)
// {
//     u8 k;

//     for(k = 0; k < bold; k++)
//     {
//         if(x1 >= k && y1 >= k)
//         {
//             LCD_DrawRectangle(x1 - k, y1 - k, x2 + k, y2 + k, color);
//         }
//     }
// }

// void ui_parameter_select_box_bold(int mode)
// {
//     u8 bold = 2;
//     int box_margin = 3;

//     // ==============================
//     // 必须和 ui_speed_page()/ui_distance_page() 保持一致
//     // ==============================
//     int pid_offset_y = 32;

//     int item_w = 70;
//     int item_h = 28;
//     int item_y = 72 + pid_offset_y;

//     int gap = 14;

//     int total_w = item_w * 3 + gap * 2;
//     int p_x = (LCD_W - total_w) / 2;
//     int i_x = p_x + item_w + gap;
//     int d_x = i_x + item_w + gap;

//     // ==============================
//     // P/I/D 加粗框坐标
//     // ==============================
//     int p_x1 = p_x - box_margin;
//     int p_y1 = item_y - box_margin;
//     int p_x2 = p_x + item_w - 1 + box_margin;
//     int p_y2 = item_y + item_h - 1 + box_margin;

//     int i_x1 = i_x - box_margin;
//     int i_y1 = item_y - box_margin;
//     int i_x2 = i_x + item_w - 1 + box_margin;
//     int i_y2 = item_y + item_h - 1 + box_margin;

//     int d_x1 = d_x - box_margin;
//     int d_y1 = item_y - box_margin;
//     int d_x2 = d_x + item_w - 1 + box_margin;
//     int d_y2 = item_y + item_h - 1 + box_margin;

//     // ==============================
//     // Target 加粗框坐标
//     // 对应你底部 Target: 文字和数值区域
//     // ==============================
//     int target_x1 = LCD_W - 150 - box_margin;
//     int target_y1 = 136 - box_margin;
//     int target_x2 = LCD_W - 4;
//     int target_y2 = 159 + box_margin;

//     switch(mode)
//     {
//         case 0: // P
//             LCD_DrawRectangle_Bold(p_x1, p_y1, p_x2, p_y2, bold, WHITE);
//             break;

//         case 1: // I
//             LCD_DrawRectangle_Bold(i_x1, i_y1, i_x2, i_y2, bold, WHITE);
//             break;

//         case 2: // D
//             LCD_DrawRectangle_Bold(d_x1, d_y1, d_x2, d_y2, bold, WHITE);
//             break;

//         case 3: // Target
//             LCD_DrawRectangle_Bold(target_x1, target_y1, target_x2, target_y2, bold, WHITE);
//             break;

//         case 4: // 清除全部
//             LCD_DrawRectangle_Bold(p_x1, p_y1, p_x2, p_y2, bold, BLACK);
//             LCD_DrawRectangle_Bold(i_x1, i_y1, i_x2, i_y2, bold, BLACK);
//             LCD_DrawRectangle_Bold(d_x1, d_y1, d_x2, d_y2, bold, BLACK);
//             LCD_DrawRectangle_Bold(target_x1, target_y1, target_x2, target_y2, bold, BLACK);
//             break;

//         default:
//             break;
//     }
// }

// //绘制速度参数选择框
// // mode = 0 选择P值显示选择框
// // mode = 1 选择I值显示选择框
// // mode = 2 选择D值显示选择框
// // mode = 3 选择target值显示选择框
// // mode = 4 全部不显示选择框
// void ui_speed_page_select_box(int mode)
// {
//     int select_box_interval = 3;
//     int line_length = 10;

//     // ==============================
//     // 必须和 ui_speed_page() 保持一致
//     // ==============================
//     int pid_offset_y = 32;

//     int item_w = 70;
//     int item_h = 28;
//     int item_y = 72 + pid_offset_y;

//     int gap = 14;

//     int total_w = item_w * 3 + gap * 2;
//     int p_x = (LCD_W - total_w) / 2;
//     int i_x = p_x + item_w + gap;
//     int d_x = i_x + item_w + gap;

//     // Target 区域
//     int target_x = LCD_W - 150;
//     int target_y = 136;
//     int target_w = 146;
//     int target_h = 24;

//     switch(mode)
//     {
//         case 0: // P
//             disp_select_box(p_x, item_w, item_y, item_h, line_length, select_box_interval, WHITE);
//             disp_select_box(i_x, item_w, item_y, item_h, line_length, select_box_interval, BLACK);
//             disp_select_box(d_x, item_w, item_y, item_h, line_length, select_box_interval, BLACK);
//             disp_select_box(target_x, target_w, target_y, target_h, line_length, select_box_interval, BLACK);
//             break;

//         case 1: // I
//             disp_select_box(p_x, item_w, item_y, item_h, line_length, select_box_interval, BLACK);
//             disp_select_box(i_x, item_w, item_y, item_h, line_length, select_box_interval, WHITE);
//             disp_select_box(d_x, item_w, item_y, item_h, line_length, select_box_interval, BLACK);
//             disp_select_box(target_x, target_w, target_y, target_h, line_length, select_box_interval, BLACK);
//             break;

//         case 2: // D
//             disp_select_box(p_x, item_w, item_y, item_h, line_length, select_box_interval, BLACK);
//             disp_select_box(i_x, item_w, item_y, item_h, line_length, select_box_interval, BLACK);
//             disp_select_box(d_x, item_w, item_y, item_h, line_length, select_box_interval, WHITE);
//             disp_select_box(target_x, target_w, target_y, target_h, line_length, select_box_interval, BLACK);
//             break;

//         case 3: // Target
//             disp_select_box(p_x, item_w, item_y, item_h, line_length, select_box_interval, BLACK);
//             disp_select_box(i_x, item_w, item_y, item_h, line_length, select_box_interval, BLACK);
//             disp_select_box(d_x, item_w, item_y, item_h, line_length, select_box_interval, BLACK);
//             disp_select_box(target_x, target_w, target_y, target_h, line_length, select_box_interval, WHITE);
//             break;

//         case 4: // all clean
//             disp_select_box(p_x, item_w, item_y, item_h, line_length, select_box_interval, BLACK);
//             disp_select_box(i_x, item_w, item_y, item_h, line_length, select_box_interval, BLACK);
//             disp_select_box(d_x, item_w, item_y, item_h, line_length, select_box_interval, BLACK);
//             disp_select_box(target_x, target_w, target_y, target_h, line_length, select_box_interval, BLACK);
//             break;

//         default:
//             break;
//     }
// }





// // 绘制定距页界面
// void ui_distance_page(void)
// {
//     TXT_OBJECT p = {0};
//     TXT_OBJECT i = {0};
//     TXT_OBJECT d = {0};

//     // 关闭背光
//     LCD_BLK_Clr();

//     // 绘制全局背景
//     LCD_Fill(0, 0, LCD_W - 1, LCD_H - 1, BLACK);

//     // ==============================
//     // PID 参数区域布局
//     // ==============================
//     int pid_offset_y = 32;   // 整体下移像素，想更低就调大

//     int item_w = 70;
//     int item_h = 28;
//     int item_y = 72 + pid_offset_y;

//     int title_y = 50 + pid_offset_y;
//     int font_size = 16;
//     int char_w = font_size / 2;

//     int gap = 14;

//     int total_w = item_w * 3 + gap * 2;
//     int p_x = (LCD_W - total_w) / 2;
//     int i_x = p_x + item_w + gap;
//     int d_x = i_x + item_w + gap;

//     // P 参数框
//     p.start_x = p_x;
//     p.start_y = item_y;
//     p.end_x = p.start_x + item_w - 1;
//     p.end_y = p.start_y + item_h - 1;
//     p.center_x = p.start_x + item_w / 2;
//     p.center_y = p.start_y + item_h / 2;

//     // I 参数框
//     i.start_x = i_x;
//     i.start_y = item_y;
//     i.end_x = i.start_x + item_w - 1;
//     i.end_y = i.start_y + item_h - 1;
//     i.center_x = i.start_x + item_w / 2;
//     i.center_y = i.start_y + item_h / 2;

//     // D 参数框
//     d.start_x = d_x;
//     d.start_y = item_y;
//     d.end_x = d.start_x + item_w - 1;
//     d.end_y = d.start_y + item_h - 1;
//     d.center_x = d.start_x + item_w / 2;
//     d.center_y = d.start_y + item_h / 2;

//     // ==============================
//     // 显示 P I D 标题
//     // ==============================
//     LCD_ShowChar(p.center_x - char_w / 2, title_y, 'P', WHITE, BLACK, font_size, 0);
//     LCD_ShowChar(i.center_x - char_w / 2, title_y, 'I', WHITE, BLACK, font_size, 0);
//     LCD_ShowChar(d.center_x - char_w / 2, title_y, 'D', WHITE, BLACK, font_size, 0);

//     // ==============================
//     // 绘制 P I D 参数背景框
//     // ==============================
//     LCD_ArcRect(p.start_x, p.start_y, p.end_x, p.end_y, BLUE);
//     LCD_ArcRect(i.start_x, i.start_y, i.end_x, i.end_y, BLUE);
//     LCD_ArcRect(d.start_x, d.start_y, d.end_x, d.end_y, BLUE);

//     // ==============================
//     // 参数默认值显示
//     // 后续可以替换为实际 PID 参数
//     // ==============================
//     disp_string_center_in_rect(p.start_x, p.start_y, item_w, item_h, "0.00", WHITE, BLUE, 16);
//     disp_string_center_in_rect(i.start_x, i.start_y, item_w, item_h, "0.00", WHITE, BLUE, 16);
//     disp_string_center_in_rect(d.start_x, d.start_y, item_w, item_h, "0.00", WHITE, BLUE, 16);

//     // ==============================
//     // 底部角度与目标值显示
//     // ==============================
//     LCD_ShowString(20,        136, "Angle:",  WHITE, BLACK, 24, 0);
//     LCD_ShowString(LCD_W-150, 136, "Target:", WHITE, BLACK, 24, 0);

//     // 默认数值
//     LCD_ShowString(98,        136, "0", WHITE, BLACK, 24, 0);
//     LCD_ShowString(LCD_W-54,  136, "0", WHITE, BLACK, 24, 0);

//     // 打开背光
//     LCD_BLK_Set();
// }

// void ui_distance_page_value_set(float p, float i, float d, int distance, int target, int quick_update)
// {
//     static int last_p_show;
//     static int last_i_show;
//     static int last_d_show;
//     static int last_distance;
//     static int last_target;

//     char show_buff[50] = {0};

//     // ==============================
//     // PID 参数区域布局
//     // 必须和 ui_distance_page() 保持一致
//     // ==============================
//     int pid_offset_y = 32;

//     int item_w = 70;
//     int item_h = 28;
//     int item_y = 72 + pid_offset_y;

//     int font_size = 16;

//     int gap = 14;

//     int total_w = item_w * 3 + gap * 2;
//     int p_x = (LCD_W - total_w) / 2;
//     int i_x = p_x + item_w + gap;
//     int d_x = i_x + item_w + gap;

//     // ==============================
//     // 底部 Distance / Target 数值坐标
//     // 如果你的静态页写的是 Angle，这里的 distance 实际就是 angle
//     // ==============================
//     int distance_value_x = 98;
//     int distance_value_y = 136;

//     int target_value_x = LCD_W - 54;
//     int target_value_y = 136;

//     // ==============================
//     // 浮点数转成两位小数整数，避免 float 直接比较
//     // 例如 1.23 -> 123
//     // ==============================
//     int p_show = (int)(p * 100.0f + (p >= 0 ? 0.5f : -0.5f));
//     int i_show = (int)(i * 100.0f + (i >= 0 ? 0.5f : -0.5f));
//     int d_show = (int)(d * 100.0f + (d >= 0 ? 0.5f : -0.5f));

//     if(quick_update != 1)
//     {
//         last_p_show = 0x7FFFFFFF;
//         last_i_show = 0x7FFFFFFF;
//         last_d_show = 0x7FFFFFFF;
//         last_distance = 0x7FFFFFFF;
//         last_target = 0x7FFFFFFF;
//     }

//     // ==============================
//     // 更新 P
//     // ==============================
//     if(last_p_show != p_show)
//     {
//         last_p_show = p_show;

//         sprintf(show_buff, "%.2f", p_show / 100.0f);

//         LCD_Fill(p_x + 4, item_y + 4,
//                  p_x + item_w - 5, item_y + item_h - 5,
//                  BLUE);

//         disp_string_center_in_rect(p_x, item_y, item_w, item_h,
//                                    show_buff, WHITE, BLUE, font_size);
//     }

//     // ==============================
//     // 更新 I
//     // ==============================
//     if(last_i_show != i_show)
//     {
//         last_i_show = i_show;

//         sprintf(show_buff, "%.2f", i_show / 100.0f);

//         LCD_Fill(i_x + 4, item_y + 4,
//                  i_x + item_w - 5, item_y + item_h - 5,
//                  BLUE);

//         disp_string_center_in_rect(i_x, item_y, item_w, item_h,
//                                    show_buff, WHITE, BLUE, font_size);
//     }

//     // ==============================
//     // 更新 D
//     // ==============================
//     if(last_d_show != d_show)
//     {
//         last_d_show = d_show;

//         sprintf(show_buff, "%.2f", d_show / 100.0f);

//         LCD_Fill(d_x + 4, item_y + 4,
//                  d_x + item_w - 5, item_y + item_h - 5,
//                  BLUE);

//         disp_string_center_in_rect(d_x, item_y, item_w, item_h,
//                                    show_buff, WHITE, BLUE, font_size);
//     }

//     // ==============================
//     // 更新 Distance / Angle
//     // ==============================
//     if(last_distance != distance)
//     {
//         last_distance = distance;

//         LCD_Fill(distance_value_x, distance_value_y,
//                  distance_value_x + 65, distance_value_y + 23,
//                  BLACK);

//         sprintf(show_buff, "%d", distance);

//         LCD_ShowString(distance_value_x, distance_value_y,
//                        show_buff, WHITE, BLACK, 24, 0);
//     }

//     // ==============================
//     // 更新 Target
//     // ==============================
//     if(last_target != target)
//     {
//         last_target = target;

//         LCD_Fill(target_value_x, target_value_y,
//                  target_value_x + 65, target_value_y + 23,
//                  BLACK);

//         sprintf(show_buff, "%d", target);

//         LCD_ShowString(target_value_x, target_value_y,
//                        show_buff, WHITE, BLACK, 24, 0);
//     }
// }

// //选择要显示哪一个页面
// // select_flag = 0 显示定速页
// // select_flag = 1 显示定距页
// // select_flag = 2 显示首页
// void ui_select_page_show(unsigned char select_flag)
// {
//     if( select_flag == 2 )
//     {
//         ui_home_page();
//     }
//     if( select_flag == 0 )
//     {
//         ui_speed_page();
//     }
//     if( select_flag == 1 )
//     {
//         ui_distance_page();
//     }
// }


// /*
// *   函数内容：画定速的PID波形
// *   函数参数：window_start_x 波形的起始X轴坐标
//               window_start_y 波形的起始Y轴坐标
//               window_w 波形组件的整体宽度
//               window_w 波形组件的整体高度
//               curve_color 波形线的颜色
//               background_color 波形组件背景色
//               rawValue 波形Y轴数据
// *   返回值：  当前波形的X轴坐标点
// */
// uint16_t draw_speed_curve(int window_start_x,int window_start_y,int window_w,int window_h,int curve_color,int background_color,short int rawValue)  
// {
// 	uint16_t x=0,y=0,i=0;
// 	static char firstPoint=1;   //是否是刚刚开始画,第一次进入
// 	static uint16_t lastX=0,lastY=0;

// 	//限幅最大和最小输入值
// 	if( rawValue >= window_h )
// 	{
//     rawValue = window_h;
// 	}
//     if( rawValue <= 0 )
//     {
//         rawValue = 0;
//     }

// 	//基于波形框 底部Y轴点 计算显示数据的偏移量
// 	y = ( window_start_y + window_h ) - rawValue;  	
    
// 	if(firstPoint)//如果是第一次画点，则无需连线，直接描点即可
// 	{
// 		LCD_DrawPoint(window_start_x,y,curve_color);   
// 		lastX=window_start_x;
// 		lastY=y;
// 		firstPoint=0; 
//     return 0;
// 	}

// 	//更新X轴时间线
// 	x=lastX + 1;
	
// 	if( x < window_w )  //不超过屏幕宽度
// 	{
// 			//清除当前位置的内容
// 			LCD_DrawVerrticalLine(x, window_start_y, window_start_y + window_h, background_color);
// 			//在当前位置跟之前位置之间连线
// 			LCD_DrawLine(lastX,lastY,x,y,curve_color);
// 			//下一列绘制白竖线表示X轴刷新点
// 			LCD_DrawVerrticalLine(x+1, window_start_y, window_start_y + window_h, WHITE);
// 			//更新之前的坐标为当前坐标
// 			lastX = x;
// 			lastY = y;
// 	}
// 	else  //超出屏幕宽度，清屏，从第一个点开始绘制，实现动态更新效果
// 	{    
// 			//清除第一列中之前的点
// 			LCD_DrawVerrticalLine(window_start_x , window_start_y, window_start_y + window_h, background_color);
// 			//显示当前的点
// 			LCD_DrawPoint(window_start_x, y, curve_color);   
// 			//更新之前的坐标为当前坐标
// 			lastX = window_start_x;
// 			lastY = y;
// 	}
// 	return x;
// }

// /*
// *   函数内容：画定距的PID波形
// *   函数参数：window_start_x 波形的起始X轴坐标
//               window_start_y 波形的起始Y轴坐标
//               window_w 波形组件的整体宽度
//               window_w 波形组件的整体高度
//               curve_color 波形线的颜色
//               background_color 波形组件背景色
//               rawValue 波形Y轴数据
// *   返回值：  当前波形的X轴坐标点
// */
// uint16_t draw_distance_curve(int window_start_x,int window_start_y,int window_w,int window_h,int curve_color,int background_color,short int rawValue)  
// {
// 	uint16_t x=0,y=0,i=0;
// 	static char firstPoint=1;   //是否是刚刚开始画,第一次进入
// 	static uint16_t lastX=0,lastY=0;

// 	//限幅最大和最小输入值
// 	if( rawValue >= window_h )
// 	{
// 			rawValue = window_h;
// 	}
// 	if( rawValue <= 0 )
// 	{
// 			rawValue = 0;
// 	}
    
// 	//基于波形框 底部Y轴点 计算显示数据的偏移量
// 	y = ( window_start_y + window_h ) - rawValue;  	
    
// 	if(firstPoint)//如果是第一次画点，则无需连线，直接描点即可
// 	{
// 		LCD_DrawPoint(window_start_x,y,curve_color);   
// 		lastX=window_start_x;
// 		lastY=y;
// 		firstPoint=0; 
// 		return 0;
// 	}

// 	x=lastX+1;
	
// 	if( x < window_w )  //不超过屏幕宽度
// 	{
// 			LCD_DrawVerrticalLine(x, window_start_y, window_start_y + window_h, background_color);
			
// 			LCD_DrawLine(lastX,lastY,x,y,curve_color);
// 			//清除下一个地方的显示内容
// 			LCD_DrawVerrticalLine(x+1, window_start_y, window_start_y + window_h, WHITE);
// 			lastX = x;
// 			lastY = y;
// 	}
// 	else  //超出屏幕宽度，清屏，从第一个点开始绘制，实现动态更新效果
// 	{        
// 			LCD_DrawVerrticalLine(window_start_x , window_start_y, window_start_y + window_h, background_color);
// 			LCD_DrawPoint(window_start_x, y, curve_color);   
// 			lastX = window_start_x;
// 			lastY = y;
// 	}
// 	return x;
// }

// //UI显示定速页的PID波形和参数
// void ui_speed_curve(void)
// {
//     disable_task_interrupt(); //禁止任务调度

//     ui_speed_page_value_set(get_speed_pid()->kp, get_speed_pid()->ki, get_speed_pid()->kd, get_encoder_count(), get_speed_pid()->target, 1);
//     int curve_x = 0;
//     //“+ SPEED_ENCODER_VALUE_MAX” 将编码器数值放大，去除负数
//     //“/ SPEED_WAVEFORM_REDUCTION_FACTOR 因为屏幕小放不下编码器最大值和最小值，因此做除法衰减数值
//     //绘制当前编码器数值曲线
//     curve_x = draw_speed_curve(0,0,319,80,GREEN,BLACK,( get_encoder_count() + SPEED_ENCODER_VALUE_MAX ) / SPEED_WAVEFORM_REDUCTION_FACTOR );  
    
//     //绘制目标速度的波形点
//     LCD_DrawPoint(curve_x, 80 - ((get_speed_pid_target() + SPEED_ENCODER_VALUE_MAX ) / SPEED_WAVEFORM_REDUCTION_FACTOR), RED);

//     enable_task_interrupt(); //开启任务调度
// }

// //UI显示定距页的PID波形和参数
// void ui_distance_curve(void)
// {
//     int current_angle = 0;

//     disable_task_interrupt(); //禁止任务调度
    
//     current_angle = get_temp_encoder() * DEGREES_PER_PULSE;

//     ui_distance_page_value_set(get_distance_pid()->kp, get_distance_pid()->ki, get_distance_pid()->kd,
//                             current_angle, get_distance_pid()->target, 1);

//     int curve_x = 0;
//     //绘制当前编码器数值曲线
//     curve_x = draw_distance_curve(0,0,319,80,GREEN,BLACK,( current_angle + DISTANCE_ENCODER_VALUE_MAX ) / DISTANCE_WAVEFORM_REDUCTION_FACTOR );  
//     //绘制目标速度的波形点
//     LCD_DrawPoint(curve_x, 80 - ((get_distance_pid_target() + DISTANCE_ENCODER_VALUE_MAX ) / DISTANCE_WAVEFORM_REDUCTION_FACTOR), RED);

//     enable_task_interrupt(); //开启任务调度
// }
