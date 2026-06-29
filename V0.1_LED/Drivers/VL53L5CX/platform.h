#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <stdint.h>
#include <string.h>

/* ========== 旧版类型 & 宏（新版 API 头文件必须用到）========== */
typedef struct {
    uint16_t address;
} VL53L5CX_Platform;

#define VL53L5CX_NB_TARGET_PER_ZONE  1U
/* =========================================================== */

/* I2C 默认地址 */
#define VL53L5CX_DEFAULT_ADDRESS  0x52

/* ========== 新版 ULD 平台接口 ========== */
int8_t   VL53L5CX_PlatformI2CWrite(uint16_t Dev, uint16_t Reg, uint8_t *pData, uint32_t len);
int8_t   VL53L5CX_PlatformI2CRead (uint16_t Dev, uint16_t Reg, uint8_t *pData, uint32_t len);
uint32_t VL53L5CX_PlatformGetTick (void);
void     VL53L5CX_PlatformWaitMs  (uint32_t ms);
void     VL53L5CX_PlatformReset   (uint8_t state);

/* ========== 旧版函数声明（不需要实现，保留即可）========== */
uint8_t VL53L5CX_RdByte       (VL53L5CX_Platform *p, uint16_t reg, uint8_t *val);
uint8_t VL53L5CX_WrByte       (VL53L5CX_Platform *p, uint16_t reg, uint8_t  val);
uint8_t VL53L5CX_RdMulti      (VL53L5CX_Platform *p, uint16_t reg, uint8_t *buf, uint32_t len);
uint8_t VL53L5CX_WrMulti      (VL53L5CX_Platform *p, uint16_t reg, uint8_t *buf, uint32_t len);
uint8_t VL53L5CX_Reset_Sensor (VL53L5CX_Platform *p);
void    VL53L5CX_SwapBuffer   (uint8_t *buf, uint16_t size);
uint8_t VL53L5CX_WaitMs       (VL53L5CX_Platform *p, uint32_t ms);

#endif /* _PLATFORM_H_ */