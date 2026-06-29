/**
 * @file    scene_controller.h
 * @brief   场景控制器 — 运动/跌倒检测 + L1/L2/L3 自动切换
 * 
 * 第3层：功能模块层。被 app_main.c 调度。
 */
#ifndef __SCENE_CONTROLLER_H
#define __SCENE_CONTROLLER_H

#include <stdint.h>

/* 外部可见的状态 */
extern uint8_t dynamic_level;   /* 当前功耗等级 1/2/3 */
extern uint8_t batt_pct;        /* 电池百分比（供低电提醒） */

void SceneController_Run(int16_t pitch_deg);
void SceneController_Init(void);

#endif
