#include "Encoder.h"

static volatile Encoder Encoder_A;
static volatile Encoder Encoder_B;

void Encoder_Init(void)
{
    //编码器引脚外部中断
	NVIC_ClearPendingIRQ(ENCODER_INT_IRQN);
	NVIC_EnableIRQ(ENCODER_INT_IRQN);

    Encoder_ClearTotal();
}

/******************************************************************
 * 函 数 名 称：Motor_Get_Encoder
 * 函 数 说 明：获取编码器的值
 * 函 数 形 参：dir=0获取左轮编码器值  dir=1获取右轮编码器值
 * 函 数 返 回：返回对应的编码器值
 * 作       者：LCKFB
 * 备       注：无
******************************************************************/
int Encoder_Get(int dir)
{
	if( !dir )
	{
		return Encoder_A.Obtained_Get_Encoder_Count;
	}

		return Encoder_B.Obtained_Get_Encoder_Count;
}

int Encoder_GetTotal(int dir)
{
    if (!dir)
    {
        return Encoder_A.Total_Encoder_Count;
    }

    return Encoder_B.Total_Encoder_Count;
}

void Encoder_ClearTotal(void)
{
    __disable_irq();

    Encoder_A.Should_Get_Encoder_Count = 0;
    Encoder_A.Obtained_Get_Encoder_Count = 0;
    Encoder_A.Total_Encoder_Count = 0;

    Encoder_B.Should_Get_Encoder_Count = 0;
    Encoder_B.Obtained_Get_Encoder_Count = 0;
    Encoder_B.Total_Encoder_Count = 0;

    __enable_irq();
}

int Encoder_GetAverageTotal(void)
{
    return (Encoder_A.Total_Encoder_Count + Encoder_B.Total_Encoder_Count) / 2;
}

void Encoder_UpdateSpeed(void)
{
    int speed_a;
    int speed_b;

    __disable_irq();

    speed_a = Encoder_A.Should_Get_Encoder_Count;
    speed_b = -Encoder_B.Should_Get_Encoder_Count;

    Encoder_A.Obtained_Get_Encoder_Count = speed_a;
    Encoder_B.Obtained_Get_Encoder_Count = speed_b;

    Encoder_A.Total_Encoder_Count += speed_a;
    Encoder_B.Total_Encoder_Count += speed_b;

    Encoder_A.Should_Get_Encoder_Count = 0;
    Encoder_B.Should_Get_Encoder_Count = 0;

    __enable_irq();
}


/*******************************************************
函数功能：外部中断模拟编码器信号
入口函数：无
返回  值：无
***********************************************************/
void GROUP1_IRQHandler(void)
{
    uint32_t gpio_interrup = 0;

	//获取中断信号
	gpio_interrup = DL_GPIO_getEnabledInterruptStatus(ENCODER_PORT,ENCODER_E1A_PIN|ENCODER_E1B_PIN|ENCODER_E2A_PIN|ENCODER_E2B_PIN);

    // encoderA
	if((gpio_interrup & ENCODER_E1A_PIN)==ENCODER_E1A_PIN)
	{
		if(!DL_GPIO_readPins(ENCODER_PORT,ENCODER_E1B_PIN))
		{
			Encoder_A.Should_Get_Encoder_Count--;
		}
		else
		{
			Encoder_A.Should_Get_Encoder_Count++;
		}
	}
	else if((gpio_interrup & ENCODER_E1B_PIN)==ENCODER_E1B_PIN)
	{
		if(!DL_GPIO_readPins(ENCODER_PORT,ENCODER_E1A_PIN))
		{
			Encoder_A.Should_Get_Encoder_Count++;
		}
		else
		{
			Encoder_A.Should_Get_Encoder_Count--;
		}
	}

	// encoderB
	if((gpio_interrup & ENCODER_E2A_PIN)==ENCODER_E2A_PIN)
	{
		if(!DL_GPIO_readPins(ENCODER_PORT,ENCODER_E2B_PIN))
		{
			Encoder_B.Should_Get_Encoder_Count--;
		}
		else
		{
			Encoder_B.Should_Get_Encoder_Count++;
		}
	}
	else if((gpio_interrup & ENCODER_E2B_PIN)==ENCODER_E2B_PIN)
	{
		if(!DL_GPIO_readPins(ENCODER_PORT,ENCODER_E2A_PIN))
		{
			Encoder_B.Should_Get_Encoder_Count++;
		}
		else
		{
			Encoder_B.Should_Get_Encoder_Count--;
		}
	}
	DL_GPIO_clearInterruptStatus(ENCODER_PORT,ENCODER_E1A_PIN|ENCODER_E1B_PIN|ENCODER_E2A_PIN|ENCODER_E2B_PIN);
}
