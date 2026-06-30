#include "power_manager.h"
#include "OLED.h"
#include <stdio.h>

#if LOW_POWER_LEVEL >= 3
#include "lpbam_driver.h"
extern uint8_t dynamic_level;
#endif

extern void SystemClock_Config(void);
extern RTC_HandleTypeDef hrtc;
extern volatile uint32_t uwTick;

/* 唤醒计数器 — Stop 后递增，供按键去抖使用 */
volatile uint32_t g_wake_count = 0;

// 运行时动态功耗级别，默认 L1
uint8_t dynamic_level = 1;
static uint32_t last_activity = 0;
static uint8_t display_on = 1;
#define DISPLAY_TIMEOUT_MS  30000U

void Power_Init(void)
{
    last_activity = HAL_GetTick();
    display_on = 1;
    dynamic_level = 1;  // 初始为全速模式
    /* Power init done */

#if LOW_POWER_LEVEL >= 3
    // 只要编译了 L3 支持，就提前初始化好 LPBAM 基础结构
    LPBAM_Driver_Init();
    LPBAM_SetPitchThreshold(15);
#endif
}

// 供外部调用的动态切换接口
void Power_SetLevel(uint8_t level)
{
    if (level >= 1 && level <= 3) {
        dynamic_level = level;
        printf("[PWR] -> L%d\n", level);
    }
}

void Power_EnterLowPower(void)
{
    // 30 秒无操作自动熄屏 — 暂时禁用
    // if (display_on && (HAL_GetTick() - last_activity > DISPLAY_TIMEOUT_MS)) {
    //     display_on = 0;
    //     OLED_Sleep();
    //     printf("[PWR] Display timeout - OLED off\n");
    // }

    // 校准期间强制走 Sleep，确保 tick 正常增长
    extern uint8_t system_ready;
    if (!system_ready) {
        __WFI();
        return;
    }

    // 根据运行时级别进入不同低功耗模式
    switch (dynamic_level)
    {
    case 1:
        // L1: 全速模式，暂不进入睡眠，避免 SysTick 唤醒失效导致系统卡死
        // __WFI();
        break;

    case 2:
        // L2: Stop 0 模式，RTC 周期唤醒 (~62.5ms)
        HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 128, RTC_WAKEUPCLOCK_RTCCLK_DIV16, 0);
        HAL_SuspendTick();
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        SystemClock_Config();
        HAL_ResumeTick();
        uwTick += 62;  /* 补偿 Stop 时间 */
        break;

    case 3:
    default:
    {
        // L3: Stop 0 + 降频唤醒 (250ms)，功耗比 L2 更低
        static uint32_t l3_wake_count = 0;
        if (l3_wake_count == 0)
            printf("[L3] Entering (RTC=512, 250ms)\n");
        HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 512, RTC_WAKEUPCLOCK_RTCCLK_DIV16, 0);
        HAL_SuspendTick();
        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        SystemClock_Config();
        HAL_ResumeTick();
        uwTick += 250; /* 补偿 Stop 时间 */
        HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 128, RTC_WAKEUPCLOCK_RTCCLK_DIV16, 0); // 恢复 62.5ms
        l3_wake_count++;
        if (l3_wake_count % 40 == 0)
            printf("[L3] Alive: %lu wakes (~%lus)\n", l3_wake_count, l3_wake_count / 4);
        break;
    }
    }
    g_wake_count++;  /* 所有等级都递增，供按键去抖 */
}

void Power_OnUserActivity(void)
{
    last_activity = HAL_GetTick();
    if (!display_on) {
        display_on = 1;
        OLED_Wake();
        printf("[PWR] User activity - OLED on\n");
    }
}

void Power_ResetActivityTimer(void)
{
    last_activity = HAL_GetTick();
}

uint8_t Power_IsDisplayOn(void)
{
    return display_on;
}
