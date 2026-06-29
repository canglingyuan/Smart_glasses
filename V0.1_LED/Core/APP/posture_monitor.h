#ifndef __POSTURE_MONITOR_H
#define __POSTURE_MONITOR_H

#include <stdint.h>

void Posture_Init(void);                    // 初始化姿态监控
void Posture_Update(int16_t pitch_deg);     // 更新姿态监控

#endif