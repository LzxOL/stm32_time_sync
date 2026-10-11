#include "stm32f10x.h"

#define PPS_TIMER_PRESCALER  (7200 - 1)  /* 72 MHz / 7200 = 10 kHz */
#define PPS_TIMER_PERIOD     (1000 - 1)  /* 10 kHz / 1000 = 10 Hz */
#define PPS_PULSE_WIDTH      (100)       /* 100 / 10 kHz = 10 ms */

static void LidarPps10Hz_Init(void)
{
	GPIO_InitTypeDef gpio_init;
	TIM_TimeBaseInitTypeDef tim_base_init;
	TIM_OCInitTypeDef tim_oc_init;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	/* TIM3 partial remap routes TIM3_CH2 to PB5. */
	GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, ENABLE);

	gpio_init.GPIO_Pin = GPIO_Pin_5;
	gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio_init);

	tim_base_init.TIM_Prescaler = PPS_TIMER_PRESCALER;
	tim_base_init.TIM_Period = PPS_TIMER_PERIOD;
	tim_base_init.TIM_ClockDivision = TIM_CKD_DIV1;
	tim_base_init.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &tim_base_init);

	TIM_OCStructInit(&tim_oc_init);
	tim_oc_init.TIM_OCMode = TIM_OCMode_PWM1;
	tim_oc_init.TIM_OutputState = TIM_OutputState_Enable;
	tim_oc_init.TIM_OCPolarity = TIM_OCPolarity_High;
	tim_oc_init.TIM_Pulse = PPS_PULSE_WIDTH;
	TIM_OC2Init(TIM3, &tim_oc_init);
	TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);

	TIM_ARRPreloadConfig(TIM3, ENABLE);
	TIM_SetCounter(TIM3, 0);
	TIM_Cmd(TIM3, ENABLE);
}

int main(void)
{
	LidarPps10Hz_Init();

	while (1)
	{
	}
}
