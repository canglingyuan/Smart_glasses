#include "platform.h"
#include "main.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c2;

/* ========== 新版平台接口 ========== */

int8_t VL53L5CX_PlatformI2CWrite(uint16_t Dev, uint16_t Reg, uint8_t *pData, uint32_t len)
{
    if (HAL_I2C_Mem_Write(&hi2c2, (Dev << 1), Reg, I2C_MEMADD_SIZE_16BIT, pData, len, 1000) == HAL_OK)
        return 0;
    else
        return -1;
}

int8_t VL53L5CX_PlatformI2CRead(uint16_t Dev, uint16_t Reg, uint8_t *pData, uint32_t len)
{
    if (HAL_I2C_Mem_Read(&hi2c2, (Dev << 1), Reg, I2C_MEMADD_SIZE_16BIT, pData, len, 1000) == HAL_OK)
        return 0;
    else
        return -1;
}

uint32_t VL53L5CX_PlatformGetTick(void)
{
    return HAL_GetTick();
}

void VL53L5CX_PlatformWaitMs(uint32_t ms)
{
    HAL_Delay(ms);
}

void VL53L5CX_PlatformReset(uint8_t state)
{
    // 如果接了 RST 引脚，在这里控制
    // HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void VL53L5CX_SwapBuffer(uint8_t *buffer, uint16_t size)
{
    uint32_t i, tmp;
    for(i = 0; i < size; i = i + 4) 
    {
        tmp = (
          buffer[i]<<24)
        |(buffer[i+1]<<16)
        |(buffer[i+2]<<8)
        |(buffer[i+3]);
        
        memcpy(&(buffer[i]), &tmp, 4);
    }
}

/* ========== 旧版平台接口（api.c 内部调用，转发到新版） ========== */

uint8_t VL53L5CX_RdByte(VL53L5CX_Platform *p_platform, uint16_t reg, uint8_t *val)
{
    return VL53L5CX_PlatformI2CRead(p_platform->address, reg, val, 1);
}

uint8_t VL53L5CX_WrByte(VL53L5CX_Platform *p_platform, uint16_t reg, uint8_t val)
{
    return VL53L5CX_PlatformI2CWrite(p_platform->address, reg, &val, 1);
}

uint8_t VL53L5CX_RdMulti(VL53L5CX_Platform *p_platform, uint16_t reg, uint8_t *buf, uint32_t len)
{
    return VL53L5CX_PlatformI2CRead(p_platform->address, reg, buf, len);
}

uint8_t VL53L5CX_WrMulti(VL53L5CX_Platform *p_platform, uint16_t reg, uint8_t *buf, uint32_t len)
{
    return VL53L5CX_PlatformI2CWrite(p_platform->address, reg, buf, len);
}

uint8_t VL53L5CX_WaitMs(VL53L5CX_Platform *p_platform, uint32_t ms)
{
    VL53L5CX_PlatformWaitMs(ms);
    return 0;
}