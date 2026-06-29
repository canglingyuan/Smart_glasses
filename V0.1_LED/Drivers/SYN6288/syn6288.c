#include "syn6288.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart2;

void SYN6288_Init(uint32_t baudrate)
{
    HAL_Delay(200); // 等待模块稳定
}

void SYN6288_Speak(char *text)
{
    /* 防止播报被打断——距上次播报至少 2.5 秒 */
    static uint32_t last_speak = 0;
    uint32_t now = HAL_GetTick();
    if (now - last_speak < 1500) {
        HAL_Delay(1500 - (uint16_t)(now - last_speak));
    }
    last_speak = HAL_GetTick();

    uint8_t frame[256];
    char full_text[256];

    // 在用户文本前加上音量控制标记 [v16]，设置最大音量
    snprintf(full_text, sizeof(full_text), "[v16]%s", text);

    uint8_t text_len = strlen(full_text);
    uint8_t data_len = text_len + 3;   // 命令字1B + 命令参数1B + 文本 + 校验1B
    uint8_t xor_check = 0;
    int i;

    frame[0] = 0xFD;
    frame[1] = (data_len >> 8) & 0xFF;
    frame[2] = data_len & 0xFF;
    frame[3] = 0x01;    // 语音合成播放命令
    frame[4] = 0x01;    // GBK编码，无背景音乐

    for (i = 0; i < text_len; i++)
        frame[5 + i] = full_text[i];

    int frame_len = 5 + text_len;
    for (i = 0; i < frame_len; i++)
        xor_check ^= frame[i];
    frame[frame_len] = xor_check;
    frame_len++;

    HAL_UART_Transmit(&huart2, frame, frame_len, HAL_MAX_DELAY);
}