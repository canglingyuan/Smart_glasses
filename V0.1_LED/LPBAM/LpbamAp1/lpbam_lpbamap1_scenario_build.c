/* USER CODE BEGIN Header */
/**
  **********************************************************************************************************************
  * @file   lpbam_lpbamap1_scenario_build.c
  * @author MCD Application Team
  * @brief  Provides LPBAM LpbamAp1 application Scenario scenario build services
  **********************************************************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  **********************************************************************************************************************
  */
/* USER CODE END Header */
/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "lpbam_lpbamap1.h"

/* Private variables -------------------------------------------------------------------------------------------------*/
/* LPBAM variables declaration */
/* USER CODE BEGIN LpbamAp1_Scenario_Descs 0 */

/* USER CODE END LpbamAp1_Scenario_Descs 0 */

/* USER CODE BEGIN Queue1_Q_Master_Receive_Config_1_Desc */

/* USER CODE END Queue1_Q_Master_Receive_Config_1_Desc */
static LPBAM_I2C_MasterRxConfigDesc_t Queue1_Q_Master_Receive_Config_1_Desc;

/* USER CODE BEGIN LpbamAp1_Scenario_Descs 1 */

/* USER CODE END LpbamAp1_Scenario_Descs 1 */

/* Exported variables ------------------------------------------------------------------------------------------------*/
/* LPBAM queues declaration */
DMA_QListTypeDef Queue1_Q;

/* External variables ------------------------------------------------------------------------------------------------*/
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/* Private function prototypes ---------------------------------------------------------------------------------------*/
static void MX_Queue1_Q_Build(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Exported functions ------------------------------------------------------------------------------------------------*/
/**
  * @brief LpbamAp1 application Scenario scenario build
  * @param None
  * @retval None
  */
void MX_LpbamAp1_Scenario_Build(void)
{
  /* USER CODE BEGIN LpbamAp1_Scenario_Build 0 */

  /* USER CODE END LpbamAp1_Scenario_Build 0 */

  /* LPBAM build Queue1 queue */
  MX_Queue1_Q_Build();

  /* USER CODE BEGIN LpbamAp1_Scenario_Build 1 */

  /* USER CODE END LpbamAp1_Scenario_Build 1 */
}

/* Private functions -------------------------------------------------------------------------------------------------*/

/**
  * @brief  LpbamAp1 application Scenario scenario Queue1 queue build
  * @param  None
  * @retval None
  */
static void MX_Queue1_Q_Build(void)
{
  /* LPBAM build variable */
  LPBAM_DMAListInfo_t pDMAListInfo_I2C = {0};
  LPBAM_I2C_ConfigAdvConf_t pRxConfig_I2C = {0};
  LPBAM_COMMON_TrigAdvConf_t pTrigConfig_I2C = {0};

  /**
    * Queue1 queue Master_Receive_Config_1 build
    */
   pDMAListInfo_I2C.QueueType= LPBAM_LINEAR_ADDRESSING_Q;
   pDMAListInfo_I2C.pInstance= LPDMA1;
  pRxConfig_I2C.AutoModeConf.TriggerState = LPBAM_I2C_AUTO_MODE_ENABLE;
  pRxConfig_I2C.AutoModeConf.TriggerSelection = LPBAM_I2C_GRP2_LPTIM1_CH1_TRG;
  pRxConfig_I2C.AutoModeConf.TriggerPolarity = LPBAM_I2C_TRIG_POLARITY_RISING;
   pRxConfig_I2C.Timing = 0x30909DEC;
   pRxConfig_I2C.WakeupIT = LPBAM_I2C_IT_NONE;
  if (ADV_LPBAM_I2C_MasterRx_SetConfigQ (I2C3, &pDMAListInfo_I2C, &pRxConfig_I2C, &Queue1_Q_Master_Receive_Config_1_Desc, &Queue1_Q) != LPBAM_OK)
  {
    Error_Handler();
  }
  pTrigConfig_I2C.TriggerConfig.TriggerMode = LPBAM_DMA_TRIGM_BLOCK_TRANSFER;
  pTrigConfig_I2C.TriggerConfig.TriggerPolarity = LPBAM_DMA_TRIG_POLARITY_RISING;
  pTrigConfig_I2C.TriggerConfig.TriggerSelection = LPBAM_LPDMA1_TRIGGER_LPTIM1_CH1;
  if (ADV_LPBAM_Q_SetTriggerConfig (&pTrigConfig_I2C, LPBAM_I2C_MASTERRX_CONFIGQ_CONFIG_NODE, &Queue1_Q_Master_Receive_Config_1_Desc) != LPBAM_OK)
  {
    Error_Handler();
  }

  /**
    * Set circular mode
    */
  if (ADV_LPBAM_Q_SetCircularMode(&Queue1_Q_Master_Receive_Config_1_Desc, LPBAM_I2C_MASTERRX_CONFIGQ_CONFIG_NODE, &Queue1_Q) != LPBAM_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN LpbamAp1_Scenario_Build */

/* USER CODE END LpbamAp1_Scenario_Build */
