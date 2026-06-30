#include "posture_monitor.h"
#include "main.h"
#include <stdio.h>
#include "stm32u5xx_hal.h"
#include "syn6288.h"
#include <stdlib.h>           // 添加此行以使用 abs()

extern uint8_t system_ready;

#define CALIB_DURATION        2000   // 校准采样时长（ms）
#define HEAD_DOWN_THRESHOLD   15     // 低头阈值（度）
#define HEAD_DOWN_COOLDOWN    10000  // 语音播报冷却时间（ms）
#define WARMUP_MS             1500   // 传感器预热时间（ms）
#define SETTLE_MS             8000   // 首个有效数据后稳定等待时间（ms）
#define FORCE_READY_TIMEOUT   20000  // 超时强制完成校准（ms）

// 佩戴平视时的经验补偿值（根据你系统日志 pitch=22 设定）
// 如果你重新佩戴校准，这个值会被自动计算，但若校准姿态不对（如放桌上），
// 且计算出的基值偏离零位太大，我们就用这个固定值确保系统能立即正常使用。
#define DEFAULT_HEAD_LEVEL_PITCH  22

static int16_t calibrated_pitch = 0;
static uint8_t  calib_done = 0;
static uint32_t calib_start = 0;
static uint32_t head_down_start = 0;
static uint8_t  calib_speech_done = 0;

void Posture_Init(void)
{
    calibrated_pitch = 0;
    calib_done = 0;
    calib_start = 0;
    head_down_start = 0;
    calib_speech_done = 0;
    printf("Posture monitor initialized.\n");
}

void Posture_Update(int16_t pitch_deg)
{
    // ================== 校准阶段 ==================
    if (calib_done == 0)
    {
        static uint16_t calib_cnt = 0;
        uint32_t now = HAL_GetTick();

        if (calib_start == 0)
        {
            calib_start = now;
            /* calib_sum removed — using direct pitch_deg */
            calib_cnt = 0;
            printf("Warming up MPU6050... (1.5s)\n");
            return;
        }

        uint32_t elapsed = now - calib_start;

        // 超时保护：8秒无有效数据，强制完成校准
        if (elapsed > FORCE_READY_TIMEOUT && calib_cnt == 0)
        {
            calibrated_pitch = DEFAULT_HEAD_LEVEL_PITCH;
            calib_done = 1;
            system_ready = 1;
            printf("Calibration timeout! Using default pitch = %d deg.\n", calibrated_pitch);
            calib_speech_done = 1;  /* 跳过语音，避免用户困惑 */
            return; 
        }

        // 阶段机：读数连续 3 秒稳定（波动 ≤ 2°）才校准完成
        static uint8_t  phase = 0;
        static int16_t  last_pitch = 0, min_pitch = 0, max_pitch = 0;
        static uint32_t stable_start = 0;

        if (phase == 0) {
            if (elapsed > WARMUP_MS) phase = 1;
        }
        if (phase == 1) {
            if (pitch_deg != 0) {
                last_pitch = min_pitch = max_pitch = pitch_deg;
                stable_start = now;
                phase = 2;
                printf("  Calib: waiting for stable readings...\n");
            }
        }
        if (phase == 2) {
            if (abs(pitch_deg - last_pitch) > 3) {
                last_pitch = min_pitch = max_pitch = pitch_deg;
                stable_start = now;
            } else {
                last_pitch = pitch_deg;
                if (pitch_deg < min_pitch) min_pitch = pitch_deg;
                if (pitch_deg > max_pitch) max_pitch = pitch_deg;
                if ((max_pitch - min_pitch) > 6) {
                    min_pitch = max_pitch = pitch_deg;
                    stable_start = now;
                }
            }
            if ((now - stable_start) > 3000) {
                calib_cnt = 1;
                phase = 3;
            }
        }

        // 稳定达标 → 完成（用当前稳定值做基准）
        if (phase == 3)
        {
            calibrated_pitch = pitch_deg;
            calib_done = 1;
            /* system_ready 延迟到语音播报之后 */
            printf("Calibration done! (stable for 3s) Base pitch = %d deg\n",
                   calibrated_pitch);
        }
        return;
    }

    // ================== 校准完成后的语音播报 ==================
    if (calib_done == 1 && calib_speech_done == 0)
    {
        HAL_Delay(500);   // 等待之前语音播报结束
        SYN6288_Speak("[v14]姿态初始化完成");
        calib_speech_done = 1;
        system_ready = 1;  // 播完才开放 OpenMV 指令
    }

    // ================== 正常工作阶段 ==================
    int16_t relative_pitch = pitch_deg - calibrated_pitch;

    /* 调试打印已移除 */

    // 低头判断（L3 模式下不播报，静默检测）
    extern uint8_t dynamic_level;
    if (abs(relative_pitch) > HEAD_DOWN_THRESHOLD && dynamic_level != 3)
    {
        if (HAL_GetTick() - head_down_start > HEAD_DOWN_COOLDOWN)
        {
            SYN6288_Speak("[v14]抬头");
            head_down_start = HAL_GetTick();
        }
    }
}