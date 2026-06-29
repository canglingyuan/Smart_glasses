/**
 * @file    lpbam_driver.h
 * @brief   L3 LPBAM driver — autonomous I2C3 → MPU6050 reads in Stop mode.
 */
#ifndef __LPBAM_DRIVER_H
#define __LPBAM_DRIVER_H

#include "main.h"

#define LPBAM_MPU6050_BUF_SIZE   8

typedef struct {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    int16_t temp;
} LPBAM_Sample_t;

void LPBAM_Driver_Init(void);
void LPBAM_Start(void);
void LPBAM_Stop(void);
uint8_t LPBAM_GetLatestSample(LPBAM_Sample_t *sample);
void LPBAM_SetPitchThreshold(int8_t deg);
uint8_t LPBAM_IsRunning(void);

#endif
