#include "mid_pid.h"

void PID_Init(PID_t *p)
{
	p->Target = 0;
	p->Actual = 0;
	p->Actual1 = 0;
	p->Out = 0;
	p->Error0 = 0;
	p->Error1 = 0;
	p->ErrorInt = 0;
}




/**
  * @brief  PID计算核心函数 (位置式)
  * @param  p: 指向 PID 结构体的指针 (包含了 Kp, Ki, Kd, 目标值, 实际值等)
  * @retval 无
  */
  
void PID_Update(PID_t *p)
{
	// 1. 更新误差历史
    // 将上一次的误差(Error0)存入 Error1，用于计算微分(D)
	p->Error1 = p->Error0;
	// 2. 计算当前误差
    // 误差 = 目标值(比如0度) - 实际值(比如当前倾角)
	p->Error0 = p->Target - p->Actual;
	// 3. 积分项处理 (I)
    // 如果启用了积分增益 (Ki不为0)
	if (p->Ki != 0)
	{
		// 累加误差 (积分就是误差的累积)
		p->ErrorInt += p->Error0;
		if(p->ErrorInt > p->ErrorIntMax) 	{p->ErrorInt = p->ErrorIntMax;}
		if(p->ErrorInt < p->ErrorIntMin)  	{p->ErrorInt = p->ErrorIntMin;}
	}
	else
	{
		// 如果 Ki 为 0，清除积分历史，方便调试时重置
		p->ErrorInt = 0;
	}
	
	// 4. 计算 PID 总输出
    // 公式：Output = (Kp * 误差) + (Ki * 累计误差) + (Kd * 误差变化率)
	
	p->Out = p->Kp * p->Error0
		   + p->Ki * p->ErrorInt
		   - p->Kd * (p->Actual - p->Actual1);
	// 5. 输出限幅
    // 防止计算出的 PWM 超过电机允许的最大值 (例如 ±100 或 ±7200)
	if (p->Out > p->OutMax) {p->Out = p->OutMax;}
	if (p->Out < p->OutMin) {p->Out = p->OutMin;}

	p->Actual1 = p->Actual;
}

void PID_UpdateWithRate(PID_t *p, float actual_rate, float dt_s)
{
	p->Error1 = p->Error0;
	p->Error0 = p->Target - p->Actual;

	if (p->Ki != 0.0f)
	{
		p->ErrorInt += p->Error0 * dt_s;
		if (p->ErrorInt > p->ErrorIntMax)
		{
			p->ErrorInt = p->ErrorIntMax;
		}
		if (p->ErrorInt < p->ErrorIntMin)
		{
			p->ErrorInt = p->ErrorIntMin;
		}
	}
	else
	{
		p->ErrorInt = 0.0f;
	}

	/*
	 * actual_rate has the same sign as Actual:
	 * moving toward +position produces a negative damping term.
	 */
	p->Out = p->Kp * p->Error0
		   + p->Ki * p->ErrorInt
		   - p->Kd * actual_rate;

	if (p->Out > p->OutMax)
	{
		p->Out = p->OutMax;
	}
	if (p->Out < p->OutMin)
	{
		p->Out = p->OutMin;
	}

	p->Actual1 = p->Actual;
}

