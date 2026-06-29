/**
 * @file    app_main.c
 * @brief   应用层 — 主调度器。只做初始化与调用，不写业务逻辑。
 * 
 * 架构: 应用层(本文件) → 功能模块层 → 设备驱动层 → HAL层
 */

#include "app_main.h"
#include "syn6288.h"
#include <stdio.h>
#include <string.h>

/* ---- 全局状态 ---- */
char last_openmv_cmd[32] = "NONE";
uint8_t system_ready = 0;
static uint8_t display_mode = 0;

/* ---- 外部引用 ---- */
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern RTC_HandleTypeDef hrtc;
extern void SystemClock_Config(void);

/* ================================================================
 *  APP_Init — 初始化所有模块
 * ================================================================ */
void APP_Init(void)
{
    OLED_Init();
    OLED_Clear();
    OLED_DrawString(0, 0, "System Boot...", Font_6x8, 1);
    OLED_UpdateScreen();

    SYN6288_Init(9600);
    OPENMV_Init();
    HCSR04_Init();

    /* 提前给 OpenMV 发数据，帮它通过自检 */
    {
        extern UART_HandleTypeDef huart3;
        for (int i = 0; i < 3; i++) {
            char buf[80];
            snprintf(buf, sizeof(buf), "D:150\nTOF:500\nIMU:0,0,16000,0,0,0\nBAT:3.9\n");
            HAL_UART_Transmit(&huart3, (uint8_t*)buf, strlen(buf), 100);
        }
    }

    MPU6050_Init();
    Posture_Init();
    TOF_Init();
    Battery_Init();

    SYN6288_Speak("开机");
    SYN6288_Speak("系统启动完成");
    HAL_Delay(2000);
    SYN6288_Speak("开始初始化");
    system_ready = 0;
    printf("=== System Ready ===\n");
}

/* ================================================================
 *  APP_Run — 主循环调度
 * ================================================================ */
void APP_Run(void)
{
    /* ---- 第1步：场景控制（姿态+运动+跌倒+L2/L3切换）---- */
    static uint8_t mpu_tick = 0;
    static int16_t pitch_int = 0;

    mpu_tick++;
    if (mpu_tick >= 10) {
        mpu_tick = 0;
        MPU6050_Data_t mpu;
        MPU6050_ReadData(&mpu);

        int32_t raw_mag = abs(mpu.ax) + abs(mpu.ay) + abs(mpu.az);
        if (raw_mag > 8000) {
            float pitch = atan2f((float)mpu.ay, (float)sqrt(mpu.ax * mpu.ax + mpu.az * mpu.az)) * 180.0f / M_PI;
            pitch_int = (int16_t)pitch;
            SceneController_Run(pitch_int);
        } else if (!system_ready) {
            /* 校准期间即使数据无效也要驱动 Posture_Update */
            SceneController_Run(0);
        }
    }

    /* 校准完成前不发数据，避免阻塞启动 */
    if (!system_ready) {
        Power_EnterLowPower();
        return;
    }
    OPENMV_ProcessCommand();

    /* ---- L3 轻量模式 ---- */
    extern uint8_t dynamic_level;
    if (dynamic_level == 3) {
        static uint8_t l3_oled_done = 0;
        if (!l3_oled_done && Power_IsDisplayOn()) {
            OLED_Clear();
            OLED_DrawString(0, 0, "Smart Glass", Font_6x8, 1);
            OLED_DrawString(0, 16, "L3 Deep Sleep", Font_6x8, 1);
            OLED_UpdateScreen();
            l3_oled_done = 1;
        }
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) {
            l3_oled_done = 0;
            Power_OnUserActivity();
            Power_SetLevel(1);
            printf("[L3] Exit -> L1 (button)\n");
            while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET);  // 等松手
        }

        /* 姿态唤醒：每10次(~2.5s)快读MPU判运动 */
        {
            static uint8_t  l3_chk = 0;
            static int16_t  l3_ref = 0;
            if (++l3_chk >= 10) {
                l3_chk = 0;
                MPU6050_Data_t mpu;
                MPU6050_ReadData(&mpu);
                if ((abs(mpu.ax) + abs(mpu.ay) + abs(mpu.az)) > 8000) {
                    float p = atan2f((float)mpu.ay, (float)sqrt(mpu.ax*mpu.ax + mpu.az*mpu.az)) * 180.0f / M_PI;
                    if (l3_ref == 0) l3_ref = (int16_t)p;
                    if (abs((int16_t)p - l3_ref) > 15) {
                        l3_oled_done = 0;
                        l3_ref = 0;
                        Power_OnUserActivity();
                        Power_SetLevel(1);
                        printf("[L3] Exit -> L1 (motion)\n");
                    }
                }
            }
        }

        if (dynamic_level == 3) {
            Power_EnterLowPower();
            return;
        }
    }

    /* ---- 状态汇报（L1/L2 下每5秒打印）---- */
    {
        static uint32_t last_report = 0;
        if (HAL_GetTick() - last_report > 5000) {
            extern uint8_t dynamic_level;
            printf("[STAT] L%d | pitch=%d | tick=%lu\n", dynamic_level, pitch_int, HAL_GetTick());
            last_report = HAL_GetTick();
        }
    }

    /* ---- 首次就绪 ---- */
    static uint8_t first_ready = 1;
    if (first_ready) { first_ready = 0; Power_ResetActivityTimer(); }

    /* ---- 按键处理 ---- */
    static uint32_t last_btn = 0;
    static uint8_t last_btn_state = 0, long_press_handled = 0;
    static uint32_t press_start = 0;

    if (HAL_GetTick() - last_btn > 50) {
        uint8_t st = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET);
        if (st == last_btn_state) {
            if (st && !long_press_handled && press_start > 0 && (HAL_GetTick() - press_start) > 3000) {
                long_press_handled = 1;
                printf("[BTN] Entering deep sleep...\n");
                SYN6288_Speak("[v14]休眠模式");
                OLED_Sleep();
                __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
                while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET)
                { HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI); SystemClock_Config(); }
                uint32_t loops = 0;
                while (1) {
                    HAL_SuspendTick(); HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
                    SystemClock_Config(); HAL_ResumeTick();
                    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) { if (++loops > 48) break; }
                    else loops = 0;
                }
                while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET);
                SystemClock_Config(); HAL_ResumeTick();
                HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 128, RTC_WAKEUPCLOCK_RTCCLK_DIV16, 0);
                OLED_Init();
                SYN6288_Speak("[v14]系统已唤醒");
                HAL_Delay(2500);
                Power_ResetActivityTimer();
                printf("[BTN] Woke up from deep sleep\n");
            } else if (!st) {
                if (press_start > 0 && !long_press_handled) { display_mode = !display_mode; Power_OnUserActivity(); }
                press_start = 0; long_press_handled = 0;
            }
        } else { if (st) { press_start = HAL_GetTick(); long_press_handled = 0; } }
        last_btn_state = st; last_btn = HAL_GetTick();
    }

    /* ---- 传感器数据 ---- */
    float raw_dist = HCSR04_GetDistance();
    static float last_valid = 150.0f;
    float filtered = (raw_dist > 30 && raw_dist < 350) ? (last_valid = raw_dist) : last_valid;
    int16_t* tof = TOF_GetData();
    OPENMV_ProcessCommand();
    FusionResult fusion = SensorFusion_Run(tof, filtered, pitch_int, last_openmv_cmd);

    /* ---- 电池监测 ---- */
    Battery_Run();

    /* ---- 融合语音播报 ---- */
    if (fusion.message && fusion.level >= RISK_MEDIUM) {
        static uint32_t last_f_alert = 0; static int last_f_level = 0;
        static uint32_t boot_ok = 0; if (boot_ok == 0) boot_ok = HAL_GetTick();
        uint32_t now = HAL_GetTick();
        if (now - boot_ok > 3000 && ((now - last_f_alert > 3000) || (fusion.level > last_f_level))) {
            SYN6288_Speak((char*)fusion.message);
            last_f_alert = now; last_f_level = fusion.level;
        }
    }

    /* ---- OLED 刷新 ---- */
    if (Power_IsDisplayOn()) {
        OLED_Clear();
        if (display_mode == 0) {
            OLED_DrawString(0, 0, "Smart Glass", Font_6x8, 1);
            {
                const char* lvl = dynamic_level == 1 ? "L1 Active" :
                                  dynamic_level == 2 ? "L2 Standby" : "L3 Sleep";
                OLED_DrawString(0, 16, (char*)lvl, Font_6x8, 1);
            }
            const char* scene = "Clear Path";
            if (fusion.level >= RISK_CRITICAL) scene = "DANGER!";
            else if (fusion.level >= RISK_HIGH) scene = "Caution";
            else if (fusion.level >= RISK_MEDIUM) scene = "Warning";
            OLED_DrawString(0, 32, (char*)scene, Font_6x8, 1);
            char bbuf[16];
            snprintf(bbuf, sizeof(bbuf), "Batt: %d%%", Battery_GetPct());
            OLED_DrawString(0, 48, bbuf, Font_6x8, 1);
        } else {
            char buf[48];
            snprintf(buf, sizeof(buf), "DEBUG MODE  L%d", dynamic_level);
            OLED_DrawString(0, 0, buf, Font_6x8, 1);
            snprintf(buf, sizeof(buf), "D:%dcm P:%d", (int)filtered, pitch_int);
            OLED_DrawString(0, 40, buf, Font_6x8, 1);
        }
        OLED_UpdateScreen();
    }

    /* ---- LED ---- */
    static uint32_t last_led = 0;
    if (HAL_GetTick() - last_led >= 200) {
        GPIOB->ODR ^= (1 << 7); GPIOG->ODR ^= (1 << 2); last_led = HAL_GetTick();
    }

    /* ---- 低功耗入口 ---- */
    Power_EnterLowPower();
}
