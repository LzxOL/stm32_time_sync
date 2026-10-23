#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "Timer.h"
#include "TimeSync_Config.h"
#include <stdio.h>

int main(void)
{
	unsigned int display_ticks = 0;
	char camera_label[17];
	char camera_status[17];
	sprintf(camera_label, "CAM FPS%u PA0/1", (unsigned int)CAMERA_TRIGGER_HZ);
	sprintf(camera_status, "REL: CAM RUN%u ", (unsigned int)CAMERA_TRIGGER_HZ);
	OLED_Init();
	TimeSync_Init();

	OLED_ShowString(2, 1, camera_label);
	OLED_ShowString(3, 1, "LIDAR PPS1 PB5");
	OLED_ShowString(4, 1, "TX PA9 RX PA10");
	
	while (1)
	{
		TimeSync_ProcessHostCommand();
		if (display_ticks++ >= 100)
		{
			display_ticks = 0;
			if (TimeSync_CameraRunning())
				OLED_ShowString(1, 1, camera_status);
			else if (TimeSync_IsUtcValid())
				OLED_ShowString(1, 1, "REL: SYNC WAIT ");
			else
				OLED_ShowString(1, 1, "TIME: WAIT PC  ");
		}
		Delay_ms(1);
	}
}
