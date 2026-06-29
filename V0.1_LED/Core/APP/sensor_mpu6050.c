#include "sensor_mpu6050.h"
#include <stdio.h>

extern I2C_HandleTypeDef hi2c3;  // Use I2C3

// Write a byte to a register
static void MPU6050_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};
    HAL_I2C_Master_Transmit(&hi2c3, MPU6050_ADDR, buf, 2, 10);
}

// Read multiple bytes from registers (returns 0 on failure)
static void MPU6050_ReadRegs(uint8_t reg, uint8_t *data, uint8_t len)
{
    HAL_I2C_Master_Transmit(&hi2c3, MPU6050_ADDR, &reg, 1, 10);
    HAL_I2C_Master_Receive(&hi2c3, MPU6050_ADDR, data, len, 10);
}

// Test if MPU6050 is connected
uint8_t MPU6050_TestConnection(void)
{
    uint8_t who_am_i;
    MPU6050_ReadRegs(0x75, &who_am_i, 1);
    if (who_am_i == 0x68)
    {
        printf("MPU6050 connected! WHO_AM_I = 0x%02X\n", who_am_i);
        return 1;
    }
    else
    {
        printf("MPU6050 not found! WHO_AM_I = 0x%02X\n", who_am_i);
        return 0;
    }
}

// Initialize MPU6050
void MPU6050_Init(void)
{
    // Wake up the device (clear sleep bit)
    MPU6050_WriteReg(0x6B, 0x00);
    HAL_Delay(100);

    // Check connection
    MPU6050_TestConnection();
}

// Read raw sensor data
void MPU6050_ReadData(MPU6050_Data_t *data)
{
    uint8_t buf[14];
    MPU6050_ReadRegs(0x3B, buf, 14);

    data->ax   = (int16_t)((buf[0]  << 8) | buf[1]);
    data->ay   = (int16_t)((buf[2]  << 8) | buf[3]);
    data->az   = (int16_t)((buf[4]  << 8) | buf[5]);
    data->temp = (int16_t)((buf[6]  << 8) | buf[7]);
    data->gx   = (int16_t)((buf[8]  << 8) | buf[9]);
    data->gy   = (int16_t)((buf[10] << 8) | buf[11]);
    data->gz   = (int16_t)((buf[12] << 8) | buf[13]);
}