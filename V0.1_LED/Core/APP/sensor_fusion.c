#include "sensor_fusion.h"
#include "tof_sensor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ================================================================
 *  危险分区常量
 * ================================================================ */
#define ZONE_DANGER_CM    100   /* <1m 危险 */
#define ZONE_CAUTION_CM   200   /* 1-2m 注意 */
#define OVERHEAD_CM        80   /* 头顶障碍阈值 */
#define OVERHEAD_MS       500   /* 持续 500ms 确认 */
#define EMERGENCY_CM       50   /* <50cm 强制唤醒 */
#define TOF_INVALID_MAX     5   /* 连续无效帧数跳过 */

/* ================================================================
 *  静止辅助：计算 TOF 全局最近距离
 * ================================================================ */
static int16_t TOF_Closest(int16_t* tof)
{
    if (!tof) return 0;
    int16_t min = 4000;
    for (int i = 0; i < 64; i++) {
        if (tof[i] > 10 && tof[i] < min) min = tof[i];
    }
    return (min < 4000) ? min : 0;
}

/* ================================================================
 *  数据有效性检查
 * ================================================================ */
static uint8_t TOF_InvalidFrames(int16_t* tof)
{
    if (!tof) return 0;
    static uint8_t invalid_cnt = 0;
    int valid = 0;
    for (int i = 0; i < 64; i++) {
        if (tof[i] > 10 && tof[i] < 4000) valid++;
    }
    if (valid < 5) invalid_cnt++; else invalid_cnt = 0;
    return invalid_cnt;
}

/* ================================================================
 *  主融合
 * ================================================================ */
FusionResult SensorFusion_Run(
    int16_t* tof_data,
    float ultrasonic_dist,
    int head_pitch,
    const char* openmv_cmd)
{
    FusionResult result = {RISK_NONE, NULL};
    static uint32_t last_speak = 0;
    static const char* last_msg = NULL;
    int16_t tof_closest = TOF_Closest(tof_data);
    int tof_invalid = TOF_InvalidFrames(tof_data);

    /* ---- 数据有效性：TOF 连续 5 帧无效 → 跳过 TOF 判断 ---- */
    if (tof_invalid > TOF_INVALID_MAX) {
        tof_closest = 0;
    }

    /* ---- 紧急：TOF < 50cm，强制唤醒 ---- */
    if (tof_closest > 10 && tof_closest < EMERGENCY_CM * 10) {
        result.level = RISK_CRITICAL;
        result.message = "危险，停下";
        goto check_repeat;
    }

    /* ---- 台阶检测（TOF 底部 vs 上部）---- */
    {
        const char* step = TOF_DetectStep();
        if (step) {
            result.level = RISK_HIGH;
            result.message = (strcmp(step, "step_down") == 0) ? "下台阶" : "上台阶";
            goto check_repeat;
        }
    }

    /* ---- 头顶保护：超声波 < 80cm + 持续 500ms ---- */
    {
        static uint32_t overhead_start = 0;
        if (ultrasonic_dist > 20 && ultrasonic_dist < OVERHEAD_CM) {
            if (overhead_start == 0) overhead_start = HAL_GetTick();
            if (HAL_GetTick() - overhead_start > OVERHEAD_MS) {
                result.level = RISK_HIGH;
                result.message = "小心头顶";
                overhead_start = 0;
                goto check_repeat;
            }
        } else {
            overhead_start = 0;
        }
    }

    /* ---- 超声波分区 ---- */
    if (ultrasonic_dist > 20 && ultrasonic_dist < ZONE_DANGER_CM) {
        result.level = RISK_CRITICAL;
        result.message = "前方障碍";
        goto check_repeat;
    }
    if (ultrasonic_dist >= ZONE_DANGER_CM && ultrasonic_dist < ZONE_CAUTION_CM) {
        if (tof_closest > 10 && tof_closest < ZONE_DANGER_CM * 10) {
            result.level = RISK_HIGH;
            result.message = "前方障碍";
            goto check_repeat;
        }
        result.level = RISK_MEDIUM;
        result.message = "注意前方";
        goto check_repeat;
    }

    /* ---- 低头提醒（在 posture_monitor 独立处理，此处不重复）---- */

    /* ---- OpenMV ---- */
    if (!openmv_cmd) goto done;
    if      (strcmp(openmv_cmd, "RED")            == 0) { result.level = RISK_HIGH;   result.message = "红灯";       goto check_repeat; }
    else if (strcmp(openmv_cmd, "OVERHEAD")       == 0) { result.level = RISK_HIGH;   result.message = "头顶障碍";   goto check_repeat; }
    else if (strcmp(openmv_cmd, "LATERAL")        == 0) { result.level = RISK_HIGH;   result.message = "横向拦截";   goto check_repeat; }
    else if (strcmp(openmv_cmd, "CROSSWALK_END")  == 0) { result.level = RISK_HIGH;   result.message = "斑马线结束"; goto check_repeat; }
    else if (strcmp(openmv_cmd, "STAIRS_DOWN")    == 0) { result.level = RISK_HIGH;   result.message = "下楼梯";     goto check_repeat; }
    else if (strcmp(openmv_cmd, "OBSTACLE")       == 0) { result.level = RISK_MEDIUM; result.message = "前方障碍物"; goto check_repeat; }
    else if (strcmp(openmv_cmd, "PIT")            == 0) { result.level = RISK_MEDIUM; result.message = "前方有坑洼"; goto check_repeat; }
    else if (strcmp(openmv_cmd, "BUMP")           == 0) { result.level = RISK_MEDIUM; result.message = "路面凸起";   goto check_repeat; }
    else if (strcmp(openmv_cmd, "CROSSWALK_NEAR") == 0) { result.level = RISK_MEDIUM; result.message = "即将出斑马线"; goto check_repeat; }
    else if (strcmp(openmv_cmd, "TACTILE_WARN")   == 0) { result.level = RISK_MEDIUM; result.message = "偏离盲道";   goto check_repeat; }
    else if (strcmp(openmv_cmd, "OBSTACLE_NEAR")  == 0) { result.level = RISK_MEDIUM; result.message = "前方障碍物"; goto check_repeat; }
    else if (strcmp(openmv_cmd, "GREEN")          == 0) { result.level = RISK_HIGH;   result.message = "绿灯";       goto check_repeat; }
    else if (strcmp(openmv_cmd, "STAIRS_UP")      == 0) { result.level = RISK_LOW;    result.message = "上楼梯";     goto check_repeat; }
done:

    result.level = RISK_NONE;
    return result;

check_repeat:
    /* 相同消息 3 秒内不重复 */
    if (result.message) {
        uint32_t now = HAL_GetTick();
        if (last_msg && strcmp(result.message, last_msg) == 0
            && now - last_speak < 3000) {
            result.message = NULL;
            result.level = RISK_NONE;
        } else {
            last_speak = now;
            last_msg = result.message;
        }
    }
    return result;
}
