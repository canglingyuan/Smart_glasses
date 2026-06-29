#include "voice_prompts.h"
#include "syn6288.h"
#include <stdio.h>

/**
 * @brief  用中文播报距离
 * @param  dist: 距离值，单位cm。如果小于0，表示无效测量
 */
void Voice_Speak_Distance(float dist)
{
    if (dist < 0) return;  // 无效测量，不播报

    char message[64];
    int cm = (int)dist;

    if (cm < 30) {
        // 近距离：紧急提醒
        snprintf(message, sizeof(message), "[v16] 前方 %d 厘米有障碍物，请停止", cm);
    } else if (cm < 100) {
        // 中距离：注意提醒
        snprintf(message, sizeof(message), "[v14] 注意，前方 %d 厘米有物体", cm);
    } else {
        // 远距离：仅为信息提示
        snprintf(message, sizeof(message), "[v12] 前方 %d 厘米内有物体", cm);
    }

    SYN6288_Speak(message);
}

/**
 * @brief  播报障碍物警告
 */
void Voice_Speak_Obstacle(void)
{
    SYN6288_Speak("[v16] 前方有障碍物，请绕行");
}

/**
 * @brief  播报价格
 * @param  price: 价格数值，单位为元
 */
void Voice_Speak_Price(int price)
{
    char message[48];
    snprintf(message, sizeof(message), "[v14] 价格为 %d 元", price);
    SYN6288_Speak(message);
}