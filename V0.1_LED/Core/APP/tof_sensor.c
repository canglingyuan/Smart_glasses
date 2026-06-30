#include "tof_sensor.h"
#include "vl53l5cx_api.h"
#include "platform.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static VL53L5CX_Configuration Dev;
static int16_t tof_data[VL53L5CX_RESOLUTION_8X8];
static uint8_t tof_ok = 0;  /* 初始化成功标志 */

void TOF_Init(void)
{
    uint8_t status;

    // 设置正确的 I2C 地址
    Dev.platform.address = 0x29;

    // 复位传感器
    VL53L5CX_PlatformReset(0);
    VL53L5CX_PlatformWaitMs(10);
    VL53L5CX_PlatformReset(1);
    VL53L5CX_PlatformWaitMs(10);

    // 初始化
    status = vl53l5cx_init(&Dev);
    if (status != VL53L5CX_STATUS_OK) {
        printf("[TOF] Init failed! status=%d\n", status);
        return;
    }

    // 配置：8x8 分辨率，15Hz，连续模式
    status  = vl53l5cx_set_resolution(&Dev, VL53L5CX_RESOLUTION_8X8);
    status |= vl53l5cx_set_ranging_frequency_hz(&Dev, 15);
    status |= vl53l5cx_set_ranging_mode(&Dev, VL53L5CX_RANGING_MODE_CONTINUOUS);

    if (status != VL53L5CX_STATUS_OK) {
        printf("[TOF] Config failed! status=%d\n", status);
        return;
    }

    // 启动测距
    vl53l5cx_start_ranging(&Dev);
    
    // 等待传感器稳定并输出第一帧有效数据
    VL53L5CX_PlatformWaitMs(100);
    
    tof_ok = 1;
    printf("[TOF] Initialized OK\n");
}

int16_t* TOF_GetData(void)
{
    if (!tof_ok) return NULL;  /* 初始化失败，跳过 */

    VL53L5CX_ResultsData Result;
    uint8_t is_ready = 0;
    uint8_t retry = 0;
    uint8_t st;

    // 重试机制：最多尝试 3 次，每次间隔 5ms
    while (retry < 3) {
        st = vl53l5cx_check_data_ready(&Dev, &is_ready);
        if (st == VL53L5CX_STATUS_OK && is_ready) break;
        retry++;
        VL53L5CX_PlatformWaitMs(5);
    }

    if (retry >= 3) {
        return NULL;
    }

    if (vl53l5cx_get_ranging_data(&Dev, &Result) == VL53L5CX_STATUS_OK) {
        memcpy(tof_data, Result.distance_mm, sizeof(tof_data));
        return tof_data;
    }
    return NULL;
}

/* ----------------------------------------------------------------
 *  台阶检测：底部 3 行 vs 上部 3 行深度对比
 *  返回: "step_down" / "step_up" / NULL
 *  需连续 3 帧确认，避免单帧误报
 * ---------------------------------------------------------------- */
const char* TOF_DetectStep(void)
{
    if (!tof_ok) return NULL;  /* 初始化失败 */

    #define STEP_THRESHOLD_MM  150
    #define STEP_FRAMES        3

    static uint8_t confirm_cnt = 0;
    static const char* last_step = NULL;

    int32_t bottom_sum = 0, upper_sum = 0;
    int b_cnt = 0, u_cnt = 0;

    /* 底部 3 行（第 5/6/7 行，index 40~63） */
    for (int i = 40; i < 64; i++) {
        if (tof_data[i] > 0 && tof_data[i] < 4000) {
            bottom_sum += tof_data[i];
            b_cnt++;
        }
    }
    /* 上部 3 行（第 0/1/2 行，index 0~23） */
    for (int i = 0; i < 24; i++) {
        if (tof_data[i] > 0 && tof_data[i] < 4000) {
            upper_sum += tof_data[i];
            u_cnt++;
        }
    }

    if (b_cnt < 5 || u_cnt < 5) {
        confirm_cnt = 0;
        last_step = NULL;
        return NULL;
    }

    int16_t bottom_avg = (int16_t)(bottom_sum / b_cnt);
    int16_t upper_avg = (int16_t)(upper_sum / u_cnt);
    int16_t diff = bottom_avg - upper_avg;

    const char* result = NULL;
    if (abs(diff) > STEP_THRESHOLD_MM) {
        result = (diff > 0) ? "step_down" : "step_up";
    }

    if (result && result == last_step) {
        if (++confirm_cnt >= STEP_FRAMES) {
            confirm_cnt = 0;
            return result;
        }
    } else {
        confirm_cnt = 1;
        last_step = result;
    }
    return NULL;
}