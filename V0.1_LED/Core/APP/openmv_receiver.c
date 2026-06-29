#include "openmv_receiver.h"
#include "syn6288.h"
#include "voice_prompts.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart3;
extern UART_HandleTypeDef huart2;  /* 备选：USART2 收 OpenMV */
extern char last_openmv_cmd[32];

static uint8_t rx_buf[OPENMV_RX_BUF_SIZE];
static uint8_t rx_index = 0;
static uint8_t rx_complete = 0;

/* ---------- 指令解析 ---------- */
static void ParseCommand(const char *cmd)
{
    char clean[OPENMV_RX_BUF_SIZE];
    strncpy(clean, cmd, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    for (int i = 0; clean[i] != '\0'; i++) {
        if (clean[i] == '\r' || clean[i] == '\n') {
            clean[i] = '\0';
            break;
        }
    }

    printf("[OMV] %s\n", clean);
    strncpy(last_openmv_cmd, clean, sizeof(last_openmv_cmd) - 1);  // ← 加这行
    last_openmv_cmd[31] = '\0';  // 确保结尾

    if (strcmp(clean, "RED") == 0) {
        SYN6288_Speak("[v16] 前方红灯，请等待");
    }
    else if (strcmp(clean, "GREEN") == 0) {
        SYN6288_Speak("[v14] 前方绿灯，请通行");
    }
    else if (strcmp(clean, "ZEBRA") == 0) {
        SYN6288_Speak("[v14] 前方有斑马线");
    }
    else if (strcmp(clean, "OBSTACLE") == 0) {
        SYN6288_Speak("[v16] 前方有障碍物，请绕行");
    }
    else if (strcmp(clean, "PIT") == 0) {
        SYN6288_Speak("[v16] 前方有坑洼，请注意脚下");
    }
    else if (strcmp(clean, "BUMP") == 0) {
        SYN6288_Speak("[v16] 前方路面凸起，请小心");
    }
    else if (strncmp(clean, "PRICE:", 6) == 0) {
        int price = atoi(clean + 6);
        Voice_Speak_Price(price);
    }
    else if (strcmp(clean, "NONE") == 0) {
        /* 无识别结果，不播报 */
    }
    else {
        // printf("[OPENMV] Unknown command: %s\n", clean);
    }
}

/* ---------- 初始化 ---------- */
void OPENMV_Init(void)
{
    rx_index = 0;
    rx_complete = 0;
    memset(rx_buf, 0, sizeof(rx_buf));
    
    /* USART3 already initialized by CubeMX — just start listening */
    // printf("[OPENMV] USART3 polling ready (115200bps)\n");
}

/* ---------- 主循环调用 ---------- */
void OPENMV_ProcessCommand(void)
{
    /* 轮询接收 */
    static char line_buf[64];
    static uint8_t li = 0;

    /* 持续发送传感器数据给 OpenMV */
    {
        static uint32_t last_send = 0;
        if (HAL_GetTick() - last_send > 200) {
            char buf[80];
            snprintf(buf, sizeof(buf), "D:150\nTOF:500\nIMU:0,0,16000,0,0,0\nBAT:3.9\n");
            HAL_UART_Transmit(&huart3, (uint8_t*)buf, strlen(buf), 100);
            last_send = HAL_GetTick();
        }
    }

    while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE)) {
        uint8_t ch = (uint8_t)(huart3.Instance->RDR & 0xFF);
        if (ch == '\n') {
            line_buf[li] = '\0';
            // printf("[OMV] L:%s\n", line_buf);
            ParseCommand(line_buf);
            li = 0;
        } else if (ch != '\r' && li < 63) {
            line_buf[li++] = ch;
        }
    }

    // if (HAL_GetTick() - last_dbg > 3000) {
    //     printf("[OpenMV] rx=%lu\n", rx_cnt);
    //     rx_cnt = 0;
    //     last_dbg = HAL_GetTick();
    // }
}

/* ---------- HAL 接收完成回调 ---------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != &huart3) return;

    uint8_t ch = rx_buf[rx_index];
    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);

    if (ch == '\r') {
        HAL_UART_Receive_IT(&huart3, &rx_buf[rx_index], 1);
        return;
    }

    if (ch == '\n') {
        rx_complete = 1;
        /* 不重启接收，等主循环处理完再重启 */
    } else {
        if (rx_index < OPENMV_RX_BUF_SIZE - 1) {
            rx_index++;
            HAL_UART_Receive_IT(&huart3, &rx_buf[rx_index], 1);
        }
    }
}

/* ---------- HAL 错误回调 ---------- */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART3) return;

    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(huart);
    }
    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_FE)) {
        __HAL_UART_CLEAR_FEFLAG(huart);
    }

    HAL_UART_Receive_IT(&huart3, &rx_buf[rx_index], 1);
    HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
}