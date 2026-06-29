#include "hcsr04.h"
#include "filter.h"

#define TRIG_PORT       GPIOA
#define TRIG_PIN        GPIO_PIN_2    /* PA2 (PA0→HOLD_PWR) */
#define NUM_SAMPLES     5
#define TIMEOUT_US      30000        /* 30ms 超时，对应约 5m */
#define FILTER_ALPHA    0.3f

static float filtered_dist = -1.0f;

/* TIM2 句柄，定义在 main.c 里 */
extern TIM_HandleTypeDef htim2;

static volatile uint32_t echo_start = 0;
static volatile uint32_t echo_end = 0;
static volatile uint8_t echo_done = 0;

/* ---------------------------------------------------------------- */
/*  初始化                                                           */
/* ---------------------------------------------------------------- */
void HCSR04_Init(void)
{
    filtered_dist = -1.0f;
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2);
}

/* ---------------------------------------------------------------- */
/*  TIM2 输入捕获中断回调（已移除 printf）                             */
/* ---------------------------------------------------------------- */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
        if (echo_start == 0) {
            echo_start = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_FALLING);
        } else {
            echo_end = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
            echo_done = 1;
            __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_RISING);
        }
    }
}

/* ---------------------------------------------------------------- */
/*  发触发脉冲                                                       */
/* ---------------------------------------------------------------- */
static void HCSR04_Trigger(void)
{
    echo_start = 0;
    echo_end = 0;
    echo_done = 0;

    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
    for (volatile int i = 0; i < 5; i++) __NOP();
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);
    for (volatile int i = 0; i < 160; i++) __NOP();
    HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
}

/* ---------------------------------------------------------------- */
/*  读回波距离                                                       */
/* ---------------------------------------------------------------- */
static float HCSR04_ReadResult(void)
{
    if (!echo_done) return -1.0f;

    uint32_t pulse_us;
    if (echo_end > echo_start) {
        pulse_us = echo_end - echo_start;
    } else {
        pulse_us = (0xFFFFFFFF - echo_start) + echo_end + 1;
    }

    if (pulse_us > TIMEOUT_US) return -1.0f;

    return (float)pulse_us * 0.017f;
}

/* ---------------------------------------------------------------- */
/*  主接口：非阻塞状态机（加入最小测量间隔 60ms）                     */
/* ---------------------------------------------------------------- */
#define STATE_IDLE       0
#define STATE_WAITING    1

float HCSR04_GetDistance(void)
{
    static uint8_t state = STATE_IDLE;
    static float samples[NUM_SAMPLES];
    static int sample_idx = 0;
    static uint32_t trigger_tick = 0;
    static uint32_t last_trig = 0;          // 新增：记录上次触发时间

    switch (state) {
    case STATE_IDLE:
        // 确保两次测量之间至少间隔 60ms
        if (HAL_GetTick() - last_trig < 60)
            return (filtered_dist < 0) ? 0.0f : filtered_dist;
        last_trig = HAL_GetTick();

        HCSR04_Trigger();
        trigger_tick = HAL_GetTick();
        state = STATE_WAITING;
        return (filtered_dist < 0) ? 0.0f : filtered_dist;

    case STATE_WAITING:
        if (echo_done) {
            float dist = HCSR04_ReadResult();
            if (dist > 0) {
                samples[sample_idx++] = dist;
            }
            state = STATE_IDLE;
            /* 5ms 冷却，避免超声波余震干扰下一采样 */
            HAL_Delay(5);

            if (sample_idx >= NUM_SAMPLES) {
                float raw = MixedFilter(samples, NUM_SAMPLES);
                /* 物理限幅 30cm~350cm，超出保留上一次有效值 */
                if (raw < 30.0f || raw > 350.0f)
                    raw = (filtered_dist > 0) ? filtered_dist : 150.0f;
                if (filtered_dist < 0)
                    filtered_dist = raw;
                else
                    filtered_dist = LowPassFilter(raw, filtered_dist, FILTER_ALPHA);
                sample_idx = 0;
            }
        } else if (HAL_GetTick() - trigger_tick > 60) {
            state = STATE_IDLE;
        }
        return (filtered_dist < 0) ? 0.0f : filtered_dist;

    default:
        state = STATE_IDLE;
        return 0.0f;
    }
}