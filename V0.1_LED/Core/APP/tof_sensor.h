#ifndef __TOF_SENSOR_H
#define __TOF_SENSOR_H

#include <stdint.h>
#include "main.h"

void TOF_Init(void);
int16_t* TOF_GetData(void);
const char* TOF_DetectStep(void);  /* 台阶检测，返回 "step_up"/"step_down"/NULL */

#endif