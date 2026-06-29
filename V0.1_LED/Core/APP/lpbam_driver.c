/**
 * @file    lpbam_driver.c
 * @brief   L3 LPBAM — uses CubeMX-generated LPBAM scenario functions.
 *
 * CubeMX generated: MX_LpbamAp1_Init / Scenario_Init / Build / Link / Start / Stop.
 * We add our pitch threshold detection in the DMA TC callback.
 */

#include "lpbam_driver.h"
#include "lpbam_lpbamap1.h"
#include <stdio.h>
#include <string.h>

static LPBAM_Sample_t  sample_buf[LPBAM_MPU6050_BUF_SIZE];
static volatile uint8_t buf_idx   = 0;
static volatile uint8_t new_data  = 0;
static volatile uint8_t running   = 0;
static int8_t  pitch_thresh = 0;

/* CubeMX-generated LPDMA handles (HAL_DMAEx_List_Init already done in MX_LPDMA1_Init) */
extern DMA_HandleTypeDef handle_LPDMA1_Channel0;
extern DMA_HandleTypeDef handle_LPDMA1_Channel1;

void LPBAM_Driver_Init(void)
{
    buf_idx  = 0;
    new_data = 0;
    running  = 0;
    memset(sample_buf, 0, sizeof(sample_buf));

    /* Enable autonomous mode clocks */
    __HAL_RCC_LPDMA1_CLK_SLEEP_ENABLE();
    __HAL_RCC_LPDMA1_CLKAM_ENABLE();
    __HAL_RCC_SRAM4_CLK_SLEEP_ENABLE();
    __HAL_RCC_SRAM4_CLKAM_ENABLE();
    __HAL_RCC_I2C3_CLK_SLEEP_ENABLE();
    __HAL_RCC_I2C3_CLKAM_ENABLE();

    MX_LpbamAp1_Scenario_Build();
    MX_LpbamAp1_Scenario_Link(&handle_LPDMA1_Channel0);
}

void LPBAM_Start(void)
{
    if (running) return;
    buf_idx  = 0;
    new_data = 0;

    /* Enable I2C3 autonomous mode (blocked CPU reads in L3) */
    {
        I2C_AutonomousModeConfTypeDef sAuto = {0};
        extern I2C_HandleTypeDef hi2c3;
        sAuto.TriggerState     = I2C_AUTO_MODE_ENABLE;
        sAuto.TriggerSelection = I2C_GRP2_RTC_WUT_TRG;
        sAuto.TriggerPolarity  = I2C_TRIG_POLARITY_RISING;
        HAL_I2CEx_SetConfigAutonomousMode(&hi2c3, &sAuto);
    }

    MX_LpbamAp1_Scenario_Start(&handle_LPDMA1_Channel0);
    running = 1;
}

void LPBAM_Stop(void)
{
    if (!running) return;
    MX_LpbamAp1_Scenario_Stop(&handle_LPDMA1_Channel0);

    /* Disable I2C3 autonomous mode (restore CPU reads) */
    {
        I2C_AutonomousModeConfTypeDef sAuto = {0};
        extern I2C_HandleTypeDef hi2c3;
        sAuto.TriggerState = I2C_AUTO_MODE_DISABLE;
        HAL_I2CEx_SetConfigAutonomousMode(&hi2c3, &sAuto);
    }

    running = 0;
}

uint8_t LPBAM_IsRunning(void) { return running; }

uint8_t LPBAM_GetLatestSample(LPBAM_Sample_t *sample)
{
    if (!sample || !new_data) return 0;
    __disable_irq();
    uint8_t idx = (buf_idx > 0) ? (buf_idx - 1) : (LPBAM_MPU6050_BUF_SIZE - 1);
    memcpy(sample, &sample_buf[idx], sizeof(LPBAM_Sample_t));
    new_data = 0;
    __enable_irq();
    return 1;
}

void LPBAM_SetPitchThreshold(int8_t deg) { pitch_thresh = deg; }

/* DMA TC callback — filled into CubeMX's empty MX_Queue1_Q_DMA_TC_Callback.
 * Called every time a 14-byte MPU6050 read completes. */
void LPBAM_RX_Callback(void)
{
    static uint32_t cnt = 0; cnt++;
    if ((cnt & 0xF) == 0) printf("[LPBAM] RX callback #%lu\n", cnt);
    new_data = 1;
    buf_idx  = (buf_idx + 1) % LPBAM_MPU6050_BUF_SIZE;

    if (pitch_thresh > 0 && buf_idx > 0) {
        uint8_t idx = (buf_idx > 0) ? (buf_idx - 1) : (LPBAM_MPU6050_BUF_SIZE - 1);
        LPBAM_Sample_t *s = &sample_buf[idx];
        int32_t ax = s->ax, ay = s->ay, az = s->az;
        int64_t ax_sq  = (int64_t)ax * ax;
        int64_t azy_sq = (int64_t)ay * ay + (int64_t)az * az;
        int32_t th_sq  = (int32_t)pitch_thresh * pitch_thresh * 365;
        if (azy_sq > 0 && ax_sq * 100 > th_sq * azy_sq / 100)
            SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    }
}
