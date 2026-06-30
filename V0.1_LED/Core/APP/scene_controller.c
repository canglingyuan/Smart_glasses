/**
 * @file    scene_controller.c
 * @brief   场景控制器 — 跌倒检测 + 运动检测 + L1/L2/L3 自动切换
 */

#include "scene_controller.h"
#include "sensor_mpu6050.h"
#include "power_manager.h"
#include "posture_monitor.h"
#include "syn6288.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ================================================================
 *  引用
 * ================================================================ */
extern uint8_t dynamic_level;           /* power_manager.c */
extern uint8_t batt_pct;                /* app_main.c */
static uint8_t auto_sleep_done = 0;

/* ================================================================
 *  SceneController_Run
 * ================================================================ */
void SceneController_Run(int16_t pitch_deg)
{
    /* 使用 app_main.c 传入的 pitch，不重复读取 MPU6050 */
    Posture_Update((int)pitch_deg);

    /* ---- 跌倒检测（需要加速度原始值）---- */
    static MPU6050_Data_t mpu;
    MPU6050_ReadData(&mpu);
    int32_t raw_mag = abs(mpu.ax) + abs(mpu.ay) + abs(mpu.az);
    if (raw_mag > 8000) {
        static uint8_t fall_phase = 0;
        static uint32_t fall_time = 0;
        float acc = sqrtf((float)mpu.ax * mpu.ax + (float)mpu.ay * mpu.ay + (float)mpu.az * mpu.az);

        if (fall_phase == 0 && acc < 5000) {
            fall_phase = 1;
            fall_time = HAL_GetTick();
        }
        if (fall_phase == 1) {
            if (acc > 30000) {
                fall_phase = 2;
                SYN6288_Speak("[v14]需要帮助");
            } else if (HAL_GetTick() - fall_time > 1000) {
                fall_phase = 0;
            }
        }
        if (fall_phase == 2 && HAL_GetTick() - fall_time > 5000) {
            fall_phase = 0;
        }
    }

    /* ---- 运动 / 静止检测（基于传入的 pitch 变化）---- */
    static int16_t last_pitch = 0;
    int16_t pitch = (int16_t)pitch_deg;
    int16_t pitch_chg = (pitch > last_pitch) ? (int16_t)(pitch - last_pitch) : (int16_t)(last_pitch - pitch);
    last_pitch = pitch;

    static uint8_t motion_cnt = 0;
    static uint32_t still_start = 0;

    /* 运动唤醒（阈值 10° 连续 2 帧） */
    if (pitch_chg > 10) {
        if (++motion_cnt >= 2) {
            if (dynamic_level != 1) auto_sleep_done = 1;
            Power_SetLevel(1);
            Power_OnUserActivity();
            still_start = 0;
        }
    } else {
        motion_cnt = 0;
    }

    /* 自动休眠（校准完成后才生效） */
    static uint32_t boot_time = 0;
    if (boot_time == 0) boot_time = HAL_GetTick();
    extern uint8_t system_ready;
    if (system_ready && !auto_sleep_done && (HAL_GetTick() - boot_time) > 10000) {
        if (pitch_chg > 10) {
            still_start = 0;
        } else {
            if (still_start == 0) still_start = HAL_GetTick();
            uint32_t elapsed = HAL_GetTick() - still_start;
            if (dynamic_level == 1 && elapsed > 15000) {
                Power_SetLevel(2);
                still_start = HAL_GetTick();
            } else if (dynamic_level == 2 && elapsed > 15000) {
                Power_SetLevel(3);
            }
        }
    }
}

void SceneController_Init(void)
{
    dynamic_level = 1;
    auto_sleep_done = 0;
}
