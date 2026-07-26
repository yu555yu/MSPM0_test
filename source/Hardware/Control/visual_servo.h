#ifndef __VISUAL_SERVO_H
#define __VISUAL_SERVO_H

// 视觉伺服：消费 UART1 上位机偏差(bias) → 比例驱动云台 + 控制激光开火
// 第二题(基本2)：上电标定虚拟准心 → 闭环把靶心怼到准心 → 到位/超时开火
// 在 main 主循环里每圈调用一次（数据驱动：仅当 uart1_rx_flag 置位才动作）
void Aim_Update(void);

#endif
