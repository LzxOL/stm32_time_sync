#include "stm32f10x.h"
#include "Timer.h"
#include <stdio.h>
#include <string.h>

#define CAMERA_TIMER_PRESCALER      (4800 - 1)   /* 72 MHz / 4800 = 15 kHz */
#define CAMERA_TRIGGER_PERIOD       (1000 - 1)   /* 15 kHz / 1000 = 15 Hz */
#define CAMERA_TRIGGER_PULSE        (150)        /* 10 ms high pulse at 15 kHz */

#define PPS_TIMER_PRESCALER         (7200 - 1)   /* 72 MHz / 7200 = 10 kHz */
#define PPS_PERIOD                  (10000 - 1)  /* 10 kHz / 10000 = 1 Hz */
#define PPS_PULSE                   (1000)       /* 100 ms high pulse */

#define GPRMC_BAUDRATE              9600
#define GPRMC_BUFFER_SIZE           128
#define HOST_RX_BUFFER_SIZE         48

typedef struct
{
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
} DateTime;

static DateTime g_current_utc = {0, 0, 0, 0, 0, 0};
static DateTime g_pending_utc = {0, 0, 0, 0, 0, 0};
static volatile uint8_t g_current_time_valid = 0;
static volatile uint8_t g_pending_time_valid = 0;

static volatile char g_host_rx_buffer[HOST_RX_BUFFER_SIZE];
static volatile uint8_t g_host_rx_index = 0;
static volatile uint8_t g_host_command_ready = 0;

static void GprmcUart_Init(void);
static void LidarPps_Init(void);
static void Uart_SendChar(char ch);
static void Uart_SendString(const char *str);
static uint8_t NmeaChecksum(const char *sentence);
static uint8_t IsLeapYear(uint16_t year);
static uint8_t DaysInMonth(uint16_t year, uint8_t month);
static void IncrementUtcOneSecond(DateTime *time);
static void ConvertBeijingToUtc(DateTime *time);
static uint8_t ParseHostTimeCommand(const char *command, DateTime *utc_time);
static void SendGprmcSentence(void);

void CameraTrigger_Init(void)
{
	GPIO_InitTypeDef gpio_init;
	TIM_TimeBaseInitTypeDef tim_base_init;
	TIM_OCInitTypeDef tim_oc_init;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	/* PA0 -> TIM2_CH1 -> Camera 0 trigger
	   PA1 -> TIM2_CH2 -> Camera 1 trigger */
	gpio_init.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
	gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio_init);

	tim_base_init.TIM_Period = CAMERA_TRIGGER_PERIOD;
	tim_base_init.TIM_Prescaler = CAMERA_TIMER_PRESCALER;
	tim_base_init.TIM_ClockDivision = TIM_CKD_DIV1;
	tim_base_init.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &tim_base_init);

	TIM_OCStructInit(&tim_oc_init);
	tim_oc_init.TIM_OCMode = TIM_OCMode_PWM1;
	tim_oc_init.TIM_OutputState = TIM_OutputState_Enable;
	tim_oc_init.TIM_OCPolarity = TIM_OCPolarity_High;
	tim_oc_init.TIM_Pulse = CAMERA_TRIGGER_PULSE;

	TIM_OC1Init(TIM2, &tim_oc_init);
	TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);

	TIM_OC2Init(TIM2, &tim_oc_init);
	TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);

	TIM_ARRPreloadConfig(TIM2, ENABLE);
	TIM_SetCounter(TIM2, 0);
	TIM_Cmd(TIM2, ENABLE);
}

void TimeSync_Init(void)
{
	GprmcUart_Init();
	CameraTrigger_Init();
	LidarPps_Init();
}

void TimeSync_ProcessHostCommand(void)
{
	char command_buffer[HOST_RX_BUFFER_SIZE];
	DateTime parsed_utc;

	if (g_host_command_ready == 0)
	{
		return;
	}

	__disable_irq();
	strcpy(command_buffer, (const char *)g_host_rx_buffer);
	g_host_command_ready = 0;
	__enable_irq();

	if (ParseHostTimeCommand(command_buffer, &parsed_utc))
	{
		__disable_irq();
		g_pending_utc = parsed_utc;
		g_pending_time_valid = 1;
		__enable_irq();
	}
}

unsigned char TimeSync_IsUtcValid(void)
{
	return (unsigned char)((g_current_time_valid != 0) || (g_pending_time_valid != 0));
}

static void GprmcUart_Init(void)
{
	GPIO_InitTypeDef gpio_init;
	USART_InitTypeDef usart_init;
	NVIC_InitTypeDef nvic_init;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

	/* PA9 -> USART1_TX -> LiDAR GPRMC
	   PA10 -> USART1_RX -> host time command */
	gpio_init.GPIO_Pin = GPIO_Pin_9;
	gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpio_init);

	gpio_init.GPIO_Pin = GPIO_Pin_10;
	gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &gpio_init);

	USART_StructInit(&usart_init);
	usart_init.USART_BaudRate = GPRMC_BAUDRATE;
	usart_init.USART_WordLength = USART_WordLength_8b;
	usart_init.USART_StopBits = USART_StopBits_1;
	usart_init.USART_Parity = USART_Parity_No;
	usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart_init.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART1, &usart_init);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

	nvic_init.NVIC_IRQChannel = USART1_IRQn;
	nvic_init.NVIC_IRQChannelPreemptionPriority = 1;
	nvic_init.NVIC_IRQChannelSubPriority = 0;
	nvic_init.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic_init);

	USART_Cmd(USART1, ENABLE);
}

static void LidarPps_Init(void)
{
	GPIO_InitTypeDef gpio_init;
	TIM_TimeBaseInitTypeDef tim_base_init;
	TIM_OCInitTypeDef tim_oc_init;
	NVIC_InitTypeDef nvic_init;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

	/* Partial remap: TIM3_CH2 -> PB5 */
	GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, ENABLE);

	/* PB5 -> TIM3_CH2 -> LiDAR PPS */
	gpio_init.GPIO_Pin = GPIO_Pin_5;
	gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
	gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio_init);

	tim_base_init.TIM_Period = PPS_PERIOD;
	tim_base_init.TIM_Prescaler = PPS_TIMER_PRESCALER;
	tim_base_init.TIM_ClockDivision = TIM_CKD_DIV1;
	tim_base_init.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &tim_base_init);

	TIM_OCStructInit(&tim_oc_init);
	tim_oc_init.TIM_OCMode = TIM_OCMode_PWM1;
	tim_oc_init.TIM_OutputState = TIM_OutputState_Enable;
	tim_oc_init.TIM_OCPolarity = TIM_OCPolarity_High;
	tim_oc_init.TIM_Pulse = PPS_PULSE;
	TIM_OC2Init(TIM3, &tim_oc_init);
	TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);

	TIM_ClearFlag(TIM3, TIM_FLAG_Update);
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

	nvic_init.NVIC_IRQChannel = TIM3_IRQn;
	nvic_init.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_init.NVIC_IRQChannelSubPriority = 0;
	nvic_init.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic_init);

	TIM_ARRPreloadConfig(TIM3, ENABLE);
	TIM_SetCounter(TIM3, 0);
	TIM_Cmd(TIM3, ENABLE);
}

static void Uart_SendChar(char ch)
{
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
	{
	}

	USART_SendData(USART1, (uint16_t)ch);
}

static void Uart_SendString(const char *str)
{
	while (*str != '\0')
	{
		Uart_SendChar(*str);
		str++;
	}

	while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
	{
	}
}

static uint8_t NmeaChecksum(const char *sentence)
{
	uint8_t checksum = 0;

	if (sentence == 0 || sentence[0] != '$')
	{
		return 0;
	}

	sentence++;
	while (*sentence != '*' && *sentence != '\0')
	{
		checksum ^= (uint8_t)(*sentence);
		sentence++;
	}

	return checksum;
}

static uint8_t IsLeapYear(uint16_t year)
{
	if ((year % 400) == 0)
	{
		return 1;
	}
	if ((year % 100) == 0)
	{
		return 0;
	}
	return (uint8_t)((year % 4) == 0);
}

static uint8_t DaysInMonth(uint16_t year, uint8_t month)
{
	static const uint8_t month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	if (month == 0 || month > 12)
	{
		return 0;
	}
	if (month == 2 && IsLeapYear(year))
	{
		return 29;
	}
	return month_days[month - 1];
}

static void IncrementUtcOneSecond(DateTime *time)
{
	if (time == 0)
	{
		return;
	}

	time->second++;
	if (time->second < 60)
	{
		return;
	}

	time->second = 0;
	time->minute++;
	if (time->minute < 60)
	{
		return;
	}

	time->minute = 0;
	time->hour++;
	if (time->hour < 24)
	{
		return;
	}

	time->hour = 0;
	time->day++;
	if (time->day <= DaysInMonth(time->year, time->month))
	{
		return;
	}

	time->day = 1;
	time->month++;
	if (time->month <= 12)
	{
		return;
	}

	time->month = 1;
	time->year++;
}

static void ConvertBeijingToUtc(DateTime *time)
{
	uint8_t step;

	if (time == 0)
	{
		return;
	}

	for (step = 0; step < 8; step++)
	{
		if (time->hour > 0)
		{
			time->hour--;
			continue;
		}

		time->hour = 23;
		if (time->day > 1)
		{
			time->day--;
			continue;
		}

		if (time->month > 1)
		{
			time->month--;
		}
		else
		{
			time->month = 12;
			if (time->year > 0)
			{
				time->year--;
			}
		}
		time->day = DaysInMonth(time->year, time->month);
	}
}

static uint16_t ParseDecimalU16(const char *text, uint8_t length)
{
	uint8_t i;
	uint16_t value = 0;

	for (i = 0; i < length; i++)
	{
		if (text[i] < '0' || text[i] > '9')
		{
			return 0xFFFF;
		}
		value = (uint16_t)(value * 10U + (uint16_t)(text[i] - '0'));
	}

	return value;
}

static uint8_t ParseHostTimeCommand(const char *command, DateTime *utc_time)
{
	DateTime parsed_time;
	uint16_t year;
	uint16_t month;
	uint16_t day;
	uint16_t hour;
	uint16_t minute;
	uint16_t second;
	uint8_t is_beijing_time = 0;
	const char *payload = command;

	if (command == 0 || utc_time == 0)
	{
		return 0;
	}

	while (*payload == ' ' || *payload == '\t' || *payload == '$')
	{
		payload++;
	}

	if (strncmp(payload, "UTC,", 4) == 0)
	{
		payload += 4;
	}
	else if (strncmp(payload, "BJT,", 4) == 0 || strncmp(payload, "CST,", 4) == 0)
	{
		payload += 4;
		is_beijing_time = 1;
	}
	else
	{
		return 0;
	}

	if (strlen(payload) < 15 || payload[8] != ',')
	{
		return 0;
	}

	year = ParseDecimalU16(payload, 4);
	month = ParseDecimalU16(payload + 4, 2);
	day = ParseDecimalU16(payload + 6, 2);
	hour = ParseDecimalU16(payload + 9, 2);
	minute = ParseDecimalU16(payload + 11, 2);
	second = ParseDecimalU16(payload + 13, 2);

	if (year == 0xFFFF || month == 0xFFFF || day == 0xFFFF ||
		hour == 0xFFFF || minute == 0xFFFF || second == 0xFFFF)
	{
		return 0;
	}

	if (month == 0 || month > 12 || day == 0 || day > DaysInMonth(year, (uint8_t)month))
	{
		return 0;
	}

	if (hour > 23 || minute > 59 || second > 59)
	{
		return 0;
	}

	parsed_time.year = year;
	parsed_time.month = (uint8_t)month;
	parsed_time.day = (uint8_t)day;
	parsed_time.hour = (uint8_t)hour;
	parsed_time.minute = (uint8_t)minute;
	parsed_time.second = (uint8_t)second;

	if (is_beijing_time)
	{
		ConvertBeijingToUtc(&parsed_time);
	}

	/* Apply the host-provided current time on the next one-second boundary. */
	IncrementUtcOneSecond(&parsed_time);
	*utc_time = parsed_time;
	return 1;
}

static void SendGprmcSentence(void)
{
	char sentence[GPRMC_BUFFER_SIZE];
	uint8_t checksum;
	size_t sentence_length;

	if (g_current_time_valid == 0)
	{
		return;
	}

	sprintf(sentence,
	        "$GPRMC,%02u%02u%02u.00,A,2237.496474,N,11356.089515,E,0.0,225.5,%02u%02u%02u,2.3,W,A*",
	        g_current_utc.hour,
	        g_current_utc.minute,
	        g_current_utc.second,
	        g_current_utc.day,
	        g_current_utc.month,
	        (uint8_t)(g_current_utc.year % 100U));

	checksum = NmeaChecksum(sentence);
	sentence_length = strlen(sentence);
	sprintf(sentence + sentence_length, "%02X\r\n", checksum);
	Uart_SendString(sentence);
}

void USART1_IRQHandler(void)
{
	uint16_t data;
	char received_char;

	if (USART_GetITStatus(USART1, USART_IT_RXNE) == RESET)
	{
		return;
	}

	data = USART_ReceiveData(USART1);
	received_char = (char)(data & 0xFF);

	if (g_host_command_ready != 0)
	{
		return;
	}

	if (received_char == '\r' || received_char == '\n')
	{
		if (g_host_rx_index > 0)
		{
			g_host_rx_buffer[g_host_rx_index] = '\0';
			g_host_rx_index = 0;
			g_host_command_ready = 1;
		}
		return;
	}

	if (g_host_rx_index >= (HOST_RX_BUFFER_SIZE - 1))
	{
		g_host_rx_index = 0;
	}

	g_host_rx_buffer[g_host_rx_index] = received_char;
	g_host_rx_index++;
}

void TIM3_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);

		if (g_pending_time_valid != 0)
		{
			g_current_utc = g_pending_utc;
			g_current_time_valid = 1;
			g_pending_time_valid = 0;
		}
		else if (g_current_time_valid != 0)
		{
			IncrementUtcOneSecond(&g_current_utc);
		}

		SendGprmcSentence();
	}
}
