#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "Timer.h"

int main(void)
{
	unsigned int display_ticks = 0;
	OLED_Init();
	TimeSync_Init();

	OLED_ShowString(2, 1, "CAM FPS15 PA0/1");
	OLED_ShowString(3, 1, "LIDAR PPS1 PB5");
	OLED_ShowString(4, 1, "TX PA9 RX PA10");
	
	while (1)
	{
		TimeSync_ProcessHostCommand();
		if (display_ticks++ >= 100)
		{
			display_ticks = 0;
			if (TimeSync_CameraRunning())
				OLED_ShowString(1, 1, "REL: CAM RUN15 ");
			else if (TimeSync_IsUtcValid())
				OLED_ShowString(1, 1, "REL: SYNC WAIT ");
			else
				OLED_ShowString(1, 1, "TIME: WAIT PC  ");
		}
		Delay_ms(1);
	}
}
