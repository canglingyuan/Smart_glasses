#include "OLED.h"
#include "main.h"
#include <string.h>
#include <stdlib.h>
/*----------------- 软件 I2C 引脚定义 -----------------*/
#define SOFT_I2C_SCL_PIN    GPIO_PIN_6   // PC6
#define SOFT_I2C_SCL_PORT   GPIOC
#define SOFT_I2C_SDA_PIN    GPIO_PIN_8   // PC8
#define SOFT_I2C_SDA_PORT   GPIOC

#define OLED_I2C_ADDR 0x3C

uint8_t OLED_Buffer[1024];

/*----------------- 基础延时 -----------------*/
static void I2C_Delay(void) {
    for (volatile int i = 0; i < 30; i++) { __NOP(); }
}

/*----------------- SDA 方向切换 -----------------*/
static void SDA_Out(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = SOFT_I2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SOFT_I2C_SDA_PORT, &GPIO_InitStruct);
}

/*----------------- I2C 起始信号 -----------------*/
static void I2C_Start(void) {
    SDA_Out();
    HAL_GPIO_WritePin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN, GPIO_PIN_SET);
    I2C_Delay();
    HAL_GPIO_WritePin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, GPIO_PIN_RESET);
    I2C_Delay();
    HAL_GPIO_WritePin(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN, GPIO_PIN_RESET);
}

/*----------------- I2C 停止信号 -----------------*/
static void I2C_Stop(void) {
    SDA_Out();
    HAL_GPIO_WritePin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN, GPIO_PIN_SET);
    I2C_Delay();
    HAL_GPIO_WritePin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, GPIO_PIN_SET);
    I2C_Delay();
}

/*----------------- I2C 发送一个字节 -----------------*/
static void I2C_SendByte(uint8_t byte) {
    SDA_Out();
    for (uint8_t i = 0; i < 8; i++) {
        if (byte & 0x80)
            HAL_GPIO_WritePin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, GPIO_PIN_SET);
        else
            HAL_GPIO_WritePin(SOFT_I2C_SDA_PORT, SOFT_I2C_SDA_PIN, GPIO_PIN_RESET);
        byte <<= 1;
        I2C_Delay();
        HAL_GPIO_WritePin(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN, GPIO_PIN_SET);
        I2C_Delay();
        HAL_GPIO_WritePin(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN, GPIO_PIN_RESET);
        I2C_Delay();
    }
    // 应答时钟
    HAL_GPIO_WritePin(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN, GPIO_PIN_SET);
    I2C_Delay();
    HAL_GPIO_WritePin(SOFT_I2C_SCL_PORT, SOFT_I2C_SCL_PIN, GPIO_PIN_RESET);
    I2C_Delay();
}

/*----------------- 发送命令 -----------------*/
void OLED_SendCommand(uint8_t cmd) {
    I2C_Start();
    I2C_SendByte(OLED_I2C_ADDR << 1);
    I2C_SendByte(0x00);
    I2C_SendByte(cmd);
    I2C_Stop();
}

/*----------------- 发送数据 -----------------*/
void OLED_SendData(uint8_t *data, uint16_t size) {
    for (uint16_t i = 0; i < size; i++) {
        I2C_Start();
        I2C_SendByte(OLED_I2C_ADDR << 1);
        I2C_SendByte(0x40);
        I2C_SendByte(data[i]);
        I2C_Stop();
    }
}

/*----------------- 初始化 OLED -----------------*/
void OLED_Init(void) {
    HAL_Delay(200);

    OLED_SendCommand(0xAE); // 关闭显示
    OLED_SendCommand(0xD5); OLED_SendCommand(0x80);
    OLED_SendCommand(0xA8); OLED_SendCommand(0x3F);
    OLED_SendCommand(0xD3); OLED_SendCommand(0x00);
    OLED_SendCommand(0x40);
    OLED_SendCommand(0xA1);
    OLED_SendCommand(0xC8);
    OLED_SendCommand(0xDA); OLED_SendCommand(0x12);
    OLED_SendCommand(0x81); OLED_SendCommand(0xCF);
    OLED_SendCommand(0xD9); OLED_SendCommand(0xF1);
    OLED_SendCommand(0xDB); OLED_SendCommand(0x30);
    OLED_SendCommand(0xA4);
    OLED_SendCommand(0xA6);
    OLED_SendCommand(0x8D); OLED_SendCommand(0x14);
    for (volatile int i = 0; i < 100000; i++);
    OLED_SendCommand(0xAF);
    for (volatile int i = 0; i < 100000; i++);

    OLED_Clear();
    OLED_UpdateScreen();
}

/*----------------- 刷新屏幕 -----------------*/
void OLED_UpdateScreen(void) {
    for (uint8_t page = 0; page < 8; page++) {
        OLED_SendCommand(0xB0 + page);
        OLED_SendCommand(0x00);
        OLED_SendCommand(0x10);
        OLED_SendData(&OLED_Buffer[page * 128], 128);
    }
}

/*----------------- 清空缓冲区 -----------------*/
void OLED_Clear(void) {
    memset(OLED_Buffer, 0, sizeof(OLED_Buffer));
}

/*----------------- 画点 -----------------*/
void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color) {
    if (x >= 128 || y >= 64) return;
    if (color)
        OLED_Buffer[x + (y / 8) * 128] |= (1 << (y % 8));
    else
        OLED_Buffer[x + (y / 8) * 128] &= ~(1 << (y % 8));
}

/*----------------- 画线 -----------------*/
void OLED_DrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    while (1) {
        OLED_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

/*----------------- 画矩形 -----------------*/
void OLED_DrawRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color) {
    OLED_DrawLine(x0, y0, x1, y0, color);
    OLED_DrawLine(x1, y0, x1, y1, color);
    OLED_DrawLine(x1, y1, x0, y1, color);
    OLED_DrawLine(x0, y1, x0, y0, color);
}

/*----------------- 填充矩形 -----------------*/
void OLED_FillRectangle(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color) {
    for (uint8_t y = y0; y <= y1; y++)
        for (uint8_t x = x0; x <= x1; x++)
            OLED_DrawPixel(x, y, color);
}

/*----------------- 画圆 -----------------*/
void OLED_DrawCircle(uint8_t x, uint8_t y, uint8_t r, uint8_t color) {
    int f = 1 - r, ddF_x = 1, ddF_y = -2 * r, xx = 0, yy = r;
    OLED_DrawPixel(x, y + r, color); OLED_DrawPixel(x, y - r, color);
    OLED_DrawPixel(x + r, y, color); OLED_DrawPixel(x - r, y, color);
    while (xx < yy) {
        if (f >= 0) { yy--; ddF_y += 2; f += ddF_y; }
        xx++; ddF_x += 2; f += ddF_x;
        OLED_DrawPixel(x + xx, y + yy, color); OLED_DrawPixel(x - xx, y + yy, color);
        OLED_DrawPixel(x + xx, y - yy, color); OLED_DrawPixel(x - xx, y - yy, color);
        OLED_DrawPixel(x + yy, y + xx, color); OLED_DrawPixel(x - yy, y + xx, color);
        OLED_DrawPixel(x + yy, y - xx, color); OLED_DrawPixel(x - yy, y - xx, color);
    }
}

/*----------------- 显示字符 -----------------*/
void OLED_DrawChar(uint8_t x, uint8_t y, char c, FontDef font, uint8_t color) {
    if (c < 32 || c > 126) return;
    const uint8_t *char_data = &font.data[(c - 32) * font.width];
    for (uint8_t i = 0; i < font.width; i++) {
        uint8_t line = char_data[i];
        for (uint8_t j = 0; j < font.height; j++) {
            if (line & 0x01) OLED_DrawPixel(x + i, y + j, color);
            line >>= 1;
        }
    }
}

/*----------------- 显示字符串 -----------------*/
void OLED_DrawString(uint8_t x, uint8_t y, char *str, FontDef font, uint8_t color) {
    while (*str) {
        OLED_DrawChar(x, y, *str, font, color);
        x += font.width;
        str++;
    }
}
/* 关闭 OLED 显示（保持显存内容） */
void OLED_Sleep(void)
{
    OLED_SendCommand(0xAE);  // Display OFF
}

/* 打开 OLED 显示 */
void OLED_Wake(void)
{
    OLED_SendCommand(0xAF);  // Display ON
}