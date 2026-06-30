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
uint32_t omv_last_tick = 0;  /* 最后一次收到 OpenMV 指令的时刻 */
uint8_t system_ready = 0;
static uint8_t display_mode = 0;

/* 按键状态（文件作用域） */
static uint32_t btn_last_tick = 0;
static uint8_t  btn_last_st   = 0;
static uint32_t btn_press_tick = 0;

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

    /* 假数据已关闭 — OpenMV 联调期间不需要 STM32 发传感器数据 */

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
            btn_last_st = 0; btn_press_tick = 0; btn_last_tick = 0;
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
                        btn_last_st = 0; btn_press_tick = 0; btn_last_tick = 0;
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
    {
        static uint8_t  stable_cnt = 0;
        static uint8_t  stable_val = 0;
        uint8_t raw = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET);

        if (raw == stable_val) {
            if (++stable_cnt >= 2) {
                /* 连续 2 次一致 → 确认状态 */
                uint8_t st = stable_val;
                if (st == btn_last_st) {
                if (st && btn_press_tick > 0 && (HAL_GetTick() - btn_press_tick) > 3000) {
                    printf("[BTN] Entering deep sleep...\n");
                    SYN6288_Speak("[v14]休眠模式");
                    OLED_Sleep();
                    while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET);
                    SystemClock_Config();
                    uint32_t loops = 0;
                    while (1) {
                        HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
                        SystemClock_Config();
                        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) { if (++loops > 48) break; }
                        else loops = 0;
                    }
                    while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET);
                    SystemClock_Config();
                    OLED_Init();
                    SYN6288_Speak("[v14]系统已唤醒");
                    HAL_Delay(2500);
                    Power_ResetActivityTimer();
                    printf("[BTN] Woke up from deep sleep\n");
                } else if (!st && btn_press_tick > 0) {
                    display_mode = !display_mode;
                    Power_OnUserActivity();
                    btn_press_tick = 0;
                }
            } else {
                if (st) {
                    btn_press_tick = HAL_GetTick();
                } else if (btn_press_tick > 0) {
                    /* 松手即切，不等去抖 */
                    display_mode = !display_mode;
                    Power_OnUserActivity();
                    btn_press_tick = 0;
                }
            }
            btn_last_st = st;
            btn_last_tick = HAL_GetTick();
        }
        } else {
            stable_cnt = 1;
            stable_val = raw;
        }
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
            /* -- 主界面 -- */
            char buf[24];
            {
                char title[20];
                snprintf(title, sizeof(title), "Smart Glass L%d", dynamic_level);
                OLED_DrawString(0, 0, title, Font_6x8, 1);
            }

            /* 行1: 当前遭遇（按融合等级排序） */
            const char* scene = "Clear";
            const char* cmd   = last_openmv_cmd;
            /* ── CRITICAL ── */
            if      (fusion.level >= RISK_CRITICAL)             scene = "DANGER!";
            /* ── HIGH ── */
            else if (strcmp(cmd, "RED")            == 0) scene = "RED Light";
            else if (strcmp(cmd, "OVERHEAD")       == 0) scene = "Overhead!";
            else if (strcmp(cmd, "LATERAL")        == 0) scene = "Lateral";
            else if (strcmp(cmd, "CROSSWALK_END")  == 0) scene = "Crosswalk End";
            else if (strcmp(cmd, "STAIRS_DOWN")    == 0) scene = "Stairs Down";
            else if (strcmp(cmd, "GREEN")          == 0) scene = "GREEN";
            else if (fusion.level >= RISK_HIGH)               scene = "Caution";
            /* ── MEDIUM ── */
            else if (strcmp(cmd, "OBSTACLE")       == 0) scene = "Obstacle";
            else if (strcmp(cmd, "PIT")            == 0) scene = "Pit Ahead";
            else if (strcmp(cmd, "BUMP")           == 0) scene = "Bump Ahead";
            else if (strcmp(cmd, "OBSTACLE_NEAR")  == 0) scene = "Obstacle Near";
            else if (strcmp(cmd, "CROSSWALK_NEAR") == 0) scene = "Crosswalk Near";
            else if (strcmp(cmd, "TACTILE_WARN")   == 0) scene = "Off Tactile";
            else if (fusion.level >= RISK_MEDIUM)             scene = "Warning";
            /* ── LOW / INFO ── */
            else if (strcmp(cmd, "ZEBRA")          == 0) scene = "Crosswalk";
            else if (strcmp(cmd, "STAIRS_UP")      == 0) scene = "Stairs Up";
            else if (strcmp(cmd, "TACTILE")        == 0) scene = "On Tactile";
            else if (strcmp(cmd, "LEFT")           == 0) scene = "Turn Left";
            else if (strcmp(cmd, "RIGHT")          == 0) scene = "Turn Right";
            OLED_DrawString(0, 16, (char*)scene, Font_6x8, 1);

            /* 行2: 超声波距离 */
            snprintf(buf, sizeof(buf), "%dcm", (int)filtered);
            OLED_DrawString(0, 32, buf, Font_6x8, 1);

            /* 行3: 电池 */
            snprintf(buf, sizeof(buf), "Batt: %d%%", Battery_GetPct());
            OLED_DrawString(0, 48, buf, Font_6x8, 1);

        } else {
            /* -- 调试界面 -- */
            char buf[48];

            /* 行0: 传感器数据 */
            int tof0 = (tof && tof[0] > 10) ? tof[0] / 10 : 0;
            snprintf(buf, sizeof(buf), "D:%d T:%d P:%d",
                     (int)filtered, tof0, pitch_int);
            OLED_DrawString(0, 0, buf, Font_6x8, 1);

            /* 行1: 传感器状态 */
            const char* mpu = system_ready ? "OK" : "--";
            const char* tus = (tof != NULL) ? "OK" : "--";
            const char* us  = (filtered > 0) ? "OK" : "--";
            snprintf(buf, sizeof(buf), "MPU:%s TOF:%s US:%s", mpu, tus, us);
            OLED_DrawString(0, 16, buf, Font_6x8, 1);

            /* 行2: 模块状态 + 功耗等级 */
            const char* omv = (strcmp(last_openmv_cmd, "NONE") != 0)
                              ? last_openmv_cmd : "--";
            snprintf(buf, sizeof(buf), "SYN:OK MV:%s L%d", omv, dynamic_level);
            OLED_DrawString(0, 32, buf, Font_6x8, 1);

            /* 行3: 电池 + 运行时间 */
            snprintf(buf, sizeof(buf), "B:%d%% %lus",
                     Battery_GetPct(), HAL_GetTick() / 1000);
            OLED_DrawString(0, 48, buf, Font_6x8, 1);
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
