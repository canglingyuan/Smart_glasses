#ifndef __APP_MAIN_H
#define __APP_MAIN_H

#include "main.h"
#include <stdlib.h>
#include "OLED.h"
#include "syn6288.h"
#include "hcsr04.h"
#include "voice_prompts.h"
#include "filter.h"
#include "openmv_receiver.h"
#include "tof_sensor.h"
#include "sensor_mpu6050.h"
#include "posture_monitor.h"
#include "scene_controller.h"
#include "battery_monitor.h"
#include "power_manager.h"
#include "sensor_fusion.h"

void APP_Init(void);
void APP_Run(void);

#endif