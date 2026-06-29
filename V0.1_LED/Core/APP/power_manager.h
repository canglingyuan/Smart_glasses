#ifndef __POWER_MANAGER_H
#define __POWER_MANAGER_H
#include <stdint.h>

#define LOW_POWER_LEVEL  3

void Power_Init(void);
void Power_EnterLowPower(void);
void Power_OnUserActivity(void);
void Power_ResetActivityTimer(void);
void Power_SetLevel(uint8_t level);
uint8_t Power_IsDisplayOn(void);
#endif