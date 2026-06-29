/**
 * @file    battery_monitor.c
 * @brief   电池监测 — ADC 采样 + 百分比 + 低电提醒
 */

#include "battery_monitor.h"
#include "main.h"
#include "syn6288.h"
#include <stdio.h>

extern ADC_HandleTypeDef hadc1;
static uint8_t batt_pct = 100;

void Battery_Init(void)
{
    batt_pct = 100;
}

void Battery_Run(void)
{
    static uint32_t last_read = 0;
    if (HAL_GetTick() - last_read < 30000) return;  /* 每30秒读一次 */
    last_read = HAL_GetTick();

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
        uint32_t adc = HAL_ADC_GetValue(&hadc1);
        float vbat = (float)adc / 16383.0f * 3.3f * 2.0f;  /* 14-bit ADC, R1=R2=100k 分压 */
        if (vbat > 4.2f) vbat = 4.2f;
        if (vbat < 3.5f) vbat = 3.5f;
        batt_pct = (uint8_t)((vbat - 3.5f) / 0.7f * 100.0f);
    }

    /* 低电提醒 */
    static uint32_t last_warn = 0;
    if (batt_pct < 20 && HAL_GetTick() - last_warn > 300000) {
        SYN6288_Speak("[v14]电量低请充电");
        last_warn = HAL_GetTick();
    }
}

uint8_t Battery_GetPct(void)
{
    return batt_pct;
}
