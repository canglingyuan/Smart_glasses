/**
 * @file    battery_monitor.h
 * @brief   电池监测 — 采样 + 百分比 + 低电提醒
 */
#ifndef __BATTERY_MONITOR_H
#define __BATTERY_MONITOR_H

#include <stdint.h>

void Battery_Init(void);
void Battery_Run(void);   /* 每秒轮询一次 */
uint8_t Battery_GetPct(void);

#endif
