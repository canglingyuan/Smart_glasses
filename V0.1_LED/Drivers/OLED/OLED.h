#ifndef __OLED_H
#define __OLED_H

#include "main.h"
#include "fonts.h"          // 字体模块
#include <stdint.h>
#include <string.h>


/* OLED I2C 地址 */
#define OLED_I2C_ADDR 0x3C

/* 全局缓冲区（在 OLED.c 中定义） */
extern uint8_t OLED_Buffer[1024];

/*--------------- 发送命令/数据 (软件I2C版) ---------------*/
void OLED_SendCommand(uint8_t cmd);
void OLED_SendData(uint8_t *data, uint16_t size);

/*--------------- 初始化与全局操作 ---------------*/
void OLED_Init(void);
void OLED_UpdateScreen(void);
void OLED_Clear(void);

/*--------------- 基础绘图函数 ---------------*/
void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);
void OLED_DrawRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);
void OLED_FillRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);
void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t r, uint8_t color);

/*--------------- 字符与字符串显示 ---------------*/
void OLED_DrawChar(uint8_t x, uint8_t y, char c, FontDef font, uint8_t color);
void OLED_DrawString(uint8_t x, uint8_t y, char *str, FontDef font, uint8_t color);

/* 关闭 OLED 显示（保持显存内容） */
void OLED_Sleep(void);
void OLED_Wake(void);

#endif /* __OLED_H */