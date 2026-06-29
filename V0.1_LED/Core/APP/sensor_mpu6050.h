#ifndef __SENSOR_MPU6050_H
#define __SENSOR_MPU6050_H

#include "main.h"

// MPU6050 I2C address (AD0 = GND)
#define MPU6050_ADDR     (0x68 << 1)

// Data structure
typedef struct {
    int16_t ax, ay, az;     // Accelerometer
    int16_t gx, gy, gz;     // Gyroscope
    int16_t temp;           // Temperature
} MPU6050_Data_t;

void MPU6050_Init(void);
void MPU6050_ReadData(MPU6050_Data_t *data);
uint8_t MPU6050_TestConnection(void);

#endif