#ifndef __TIMER_H
#define __TIMER_H

void CameraTrigger_Init(void);
void TimeSync_Init(void);
void TimeSync_ProcessHostCommand(void);
unsigned char TimeSync_IsUtcValid(void);

#endif
