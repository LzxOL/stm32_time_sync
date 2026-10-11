#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "Timer.h"

int main(void)
{
	OLED_Init();
	TimeSync_Init();

	OLED_ShowString(2, 1, "CAM FPS15 PA0/1");
	OLED_ShowString(3, 1, "LIDAR PPS1 PB5");
	OLED_ShowString(4, 1, "TX PA9 RX PA10");
	
	while (1)
	{
		TimeSync_ProcessHostCommand();
		if (TimeSync_IsUtcValid())
		{
			OLED_ShowString(1, 1, "TIME: LOCKED  ");
		}
		else
		{
			OLED_ShowString(1, 1, "TIME: WAIT PC ");
		}
		Delay_ms(100);
	}
}
