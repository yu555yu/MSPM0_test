#include "app_irtracking.h"

/*
 * 循迹误差，全局静态变量
 * 负数：线在左边，需要向左修正
 * 正数：线在右边，需要向右修正
 * 0：居中
 */
static float LineTracking_Err = 0;

/*
 * ================ 加权平均法配置 ================
 *
 * 8个传感器从左到右排列，权重从负到正，中心对称
 * 可根据实际传感器间距调整权重分布
 */
#define TRACK_WEIGHT1   -7
#define TRACK_WEIGHT2   -5
#define TRACK_WEIGHT3   -3
#define TRACK_WEIGHT4   -1
#define TRACK_WEIGHT5    1
#define TRACK_WEIGHT6    3
#define TRACK_WEIGHT7    5
#define TRACK_WEIGHT8    7

/*
 * 误差增益：将加权平均（-7~+7）放大到转向PID的有效输入范围
 *
 * 你的实测数据：
 *   - 目标240时实际只有110 → 转向环需要较大误差才能输出足够PWM
 *   - 直线误差在 ±1.5 左右 → 加权平均后直线误差 ≈ 0，比原来更稳定
 *
 * 普通循线增益和直角弯目标分别调节：
 *   TRACK_ERROR_GAIN：普通循线加权误差增益
 *   TRACK_CORNER_YAW_RATE：直角/大弯固定角速度目标
 * 如果转弯不够猛，调大 GAIN；如果直线抖动，适当减小 GAIN
 */
#define TRACK_ERROR_GAIN       25.0f
#define TRACK_CORNER_YAW_RATE  120.0f

/*
 * 丢线保护：连续丢线多少次后，误差归零（防止沿上一次误差一直转圈）
 * 设为 0 表示永不归零，丢线时保持上一次误差
 */
#define TRACK_LOSS_TIMEOUT  0

//巡线探头的处理             //从左到右
static void track_deal_four(u8 *s1, u8 *s2, u8 *s3, u8 *s4, u8 *s5, u8 *s6, u8 *s7, u8 *s8)
{
    *s1 = DL_GPIO_readPins(Track_T1_PORT, Track_T1_PIN) ? 1 : 0;
    *s2 = DL_GPIO_readPins(Track_T2_PORT, Track_T2_PIN) ? 1 : 0;
    *s3 = DL_GPIO_readPins(Track_T3_PORT, Track_T3_PIN) ? 1 : 0;
    *s4 = DL_GPIO_readPins(Track_T4_PORT, Track_T4_PIN) ? 1 : 0;

    *s5 = DL_GPIO_readPins(Track_T5_PORT, Track_T5_PIN) ? 1 : 0;
    *s6 = DL_GPIO_readPins(Track_T6_PORT, Track_T6_PIN) ? 1 : 0;
    *s7 = DL_GPIO_readPins(Track_T7_PORT, Track_T7_PIN) ? 1 : 0;
    *s8 = DL_GPIO_readPins(Track_T8_PORT, Track_T8_PIN) ? 1 : 0;
}

void LineTracking_ShowRawOnLCD(void)
{
    u8 sensor[8];
    char buf[32];

    track_deal_four(&sensor[0], &sensor[1], &sensor[2], &sensor[3],
                    &sensor[4], &sensor[5], &sensor[6], &sensor[7]);

    sprintf(buf, "%d %d %d %d %d %d %d %d",
            sensor[0], sensor[1], sensor[2], sensor[3],
            sensor[4], sensor[5], sensor[6], sensor[7]);

    LCD_ShowString(0, 100, buf, GREEN, BLACK, 16, 0);
}

/*
 * 加权平均法循迹
 *
 * 原理：
 *   将8个传感器从左到右赋予固定权重（左负右正），
 *   检测到黑线的传感器参与加权平均计算：
 *     error = Σ(weight_i × active_i) / Σ(active_i) × GAIN
 *
 *   黑线 = 0，白底 = 1
 *   active_i = (sensor_i == 0)，即检测到黑线时为1
 *
 * 示例（权重 -7,-5,-3,-1,1,3,5,7，GAIN=60）：
 *   线居中(传感器4,5见黑)：error = (-1+1)/2 × 60 = 0
 *   线偏右(传感器5见黑)：  error = 1/1 × 60 = 60
 *   线在右边(传感器7,8见黑)：error = (5+7)/2 × 60 = 360
 *   线在最左边(传感器1见黑)：error = -7/1 × 60 = -420
 */
// x1-x8 从左往右数
void LineWalking(void)
{
    static u8 x1, x2, x3, x4, x5, x6, x7, x8;
#if TRACK_LOSS_TIMEOUT > 0
    static u8 loss_count = 0;
#endif

    track_deal_four(&x1, &x2, &x3, &x4, &x5, &x6, &x7, &x8);

    /*
     * 黑线 = 0，白底 = 1
     *
     * err > 0：车往左修正
     * err < 0：车往右修正
     */

    // ============ 直角/大弯：固定大误差，优先处理 ============

    // 左直角/左大弯：左边检测到黑线，且最右边是白底
    if ((x1 == 0 || x2 == 0) && x8 == 1)
    {
        LineTracking_Err = -TRACK_CORNER_YAW_RATE;
#if TRACK_LOSS_TIMEOUT > 0
        loss_count = 0;
#endif
    }

    // 右直角/右大弯：右边检测到黑线，且最左边是白底
    else if ((x7 == 0 || x8 == 0) && x1 == 1)
    {
        LineTracking_Err = TRACK_CORNER_YAW_RATE;
#if TRACK_LOSS_TIMEOUT > 0
        loss_count = 0;
#endif
    }

    // ============ 普通循线：加权平均 ============
    else
    {
        /*
         * 将传感器值转为"有效信号"
         * active = 1 表示检测到黑线，0 表示未检测到
         */
        u8 s1 = (x1 == 0);
        u8 s2 = (x2 == 0);
        u8 s3 = (x3 == 0);
        u8 s4 = (x4 == 0);
        u8 s5 = (x5 == 0);
        u8 s6 = (x6 == 0);
        u8 s7 = (x7 == 0);
        u8 s8 = (x8 == 0);
        /*
         * 加权平均计算
         * numerator   = Σ(weight_i × active_i)
         * denominator = Σ(active_i) = 检测到黑线的传感器数量
         */
        int numerator = (int)s1 * TRACK_WEIGHT1 + (int)s2 * TRACK_WEIGHT2
                      + (int)s3 * TRACK_WEIGHT3 + (int)s4 * TRACK_WEIGHT4
                      + (int)s5 * TRACK_WEIGHT5 + (int)s6 * TRACK_WEIGHT6
                      + (int)s7 * TRACK_WEIGHT7 + (int)s8 * TRACK_WEIGHT8;

        int denominator = (int)s1 + (int)s2 + (int)s3 + (int)s4
                        + (int)s5 + (int)s6 + (int)s7 + (int)s8;

        if (denominator > 0)
        {
            LineTracking_Err = (float)numerator / (float)denominator * TRACK_ERROR_GAIN;
#if TRACK_LOSS_TIMEOUT > 0
            loss_count = 0;
#endif
        }
#if TRACK_LOSS_TIMEOUT > 0
        else
        {
            /*
             * 丢线处理：所有传感器都在白底上
             * 短暂丢线 → 保持上一次误差继续修正
             * 连续丢线超过阈值 → 误差归零（防止绕圈）
             */
            loss_count++;
            if (loss_count >= TRACK_LOSS_TIMEOUT)
            {
                LineTracking_Err = 0;
            }
        }
#endif
    }
}


/**
 * @brief 获取循迹误差
 */
float LineTracking_GetError(void)
{
    return LineTracking_Err;
}
