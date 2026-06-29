#ifndef __SENSOR_FUSION_H
#define __SENSOR_FUSION_H

#include <stdint.h>

typedef enum {
    RISK_NONE = 0,
    RISK_LOW,       // 一般提醒（距离播报、绿灯）
    RISK_MEDIUM,    // 中等风险（斑马线、远处障碍物）
    RISK_HIGH,      // 高风险（低头、近距离障碍物）
    RISK_CRITICAL   // 紧急（脚下坑洞、极近障碍物）
} RiskLevel;

typedef struct {
    RiskLevel level;
    const char* message;   // 语音播报内容
} FusionResult;

FusionResult SensorFusion_Run(
    int16_t* tof_data,       // TOF 8x8数据
    float ultrasonic_dist,   // 超声波距离(cm)
    int head_pitch,          // 头部俯仰角
    const char* openmv_cmd   // OpenMV指令
);

#endif