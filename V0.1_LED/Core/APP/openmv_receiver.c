#include "openmv_receiver.h"
#include "syn6288.h"
#include "voice_prompts.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart3;
extern char last_openmv_cmd[32];

/* ---- 环形缓冲区 ---- */
#define RB_SIZE  1024
static uint8_t  rb_buf[RB_SIZE];
static volatile uint16_t rb_head = 0;  /* ISR 写 */
static uint16_t          rb_tail = 0;  /* 主循环读 */

/* ---- 统计 ---- */
static uint32_t first_byte_tick = 0;
static uint32_t line_count      = 0;
static uint32_t last_report     = 0;
static uint16_t rb_max_depth    = 0;

/* ---- 中断接收标志 ---- */
static volatile uint8_t it_started = 0;

/* ---------- 指令解析 ---------- */
static void ParseCommand(const char *cmd)
{
    char clean[OPENMV_RX_BUF_SIZE];
    strncpy(clean, cmd, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    for (int i = 0; clean[i] != '\0'; i++) {
        if (clean[i] == '\r' || clean[i] == '\n') {
            clean[i] = '\0';
            break;
        }
    }

    strncpy(last_openmv_cmd, clean, sizeof(last_openmv_cmd) - 1);
    last_openmv_cmd[31] = '\0';
    extern uint32_t omv_last_tick;  /* 记录最后收到指令的时间 */
    omv_last_tick = HAL_GetTick();

    if (strcmp(clean, "NONE") == 0) return;  /* 静默，不打印不统计 */

    printf("[OMV] CMD: %s\n", clean);

    /* 校准期间只记录不播报，避免打断初始化语音序列 */
    extern uint8_t system_ready;
    if (!system_ready) return;

    /* 同指令 5 秒去重 */
    {
        static char   last_spoken[32] = "";
        static uint32_t last_spoken_tick = 0;
        if (strcmp(clean, last_spoken) == 0 &&
            HAL_GetTick() - last_spoken_tick < 5000)
            return;
        strcpy(last_spoken, clean);
        last_spoken_tick = HAL_GetTick();
    }

    if      (strcmp(clean, "RED")            == 0) SYN6288_Speak("[v16] 前方红灯，请等待");
    else if (strcmp(clean, "GREEN")          == 0) SYN6288_Speak("[v14] 前方绿灯，请通行");
    else if (strcmp(clean, "ZEBRA")          == 0) SYN6288_Speak("[v14] 前方有斑马线");
    else if (strcmp(clean, "OBSTACLE")       == 0) SYN6288_Speak("[v16] 前方有障碍物，请绕行");
    else if (strcmp(clean, "PIT")            == 0) SYN6288_Speak("[v16] 前方有坑洼，请注意脚下");
    else if (strcmp(clean, "BUMP")           == 0) SYN6288_Speak("[v16] 前方路面凸起，请小心");
    else if (strcmp(clean, "OVERHEAD")       == 0) SYN6288_Speak("[v16] 小心头顶障碍");
    else if (strcmp(clean, "LATERAL")        == 0) SYN6288_Speak("[v16] 前方有横向拦截物，请绕行");
    else if (strcmp(clean, "CROSSWALK_DEVIATION")==0) SYN6288_Speak("[v14] 已偏离斑马线，请调整方向");
    else if (strcmp(clean, "CROSSWALK_END")  == 0) SYN6288_Speak("[v16] 斑马线即将结束，注意前方");
    else if (strcmp(clean, "CROSSWALK_NEAR") == 0) SYN6288_Speak("[v14] 即将走出斑马线");
    else if (strcmp(clean, "TACTILE_WARN")   == 0) SYN6288_Speak("[v14] 请回到盲道");
    else if (strcmp(clean, "TACTILE_TURN")   == 0) SYN6288_Speak("[v14] 盲道转弯，请沿盲道行走");
    else if (strcmp(clean, "OBSTACLE_NEAR")  == 0) SYN6288_Speak("[v14] 前方有障碍物，请绕行");
    else if (strcmp(clean, "STAIRS_DOWN")    == 0) SYN6288_Speak("[v16] 前方下楼梯");
    else if (strcmp(clean, "STAIRS_UP")      == 0) SYN6288_Speak("[v14] 前方上楼梯");
    else if (strcmp(clean, "LEFT")           == 0) SYN6288_Speak("[v14] 请向左绕行");
    else if (strcmp(clean, "RIGHT")          == 0) SYN6288_Speak("[v14] 请向右绕行");
    else if (strncmp(clean, "PRICE:", 6)     == 0) { int p = atoi(clean + 6); Voice_Speak_Price(p); }
    /* TACTILE (盲道中间) / NONE — 不播报 */
}

/* ==================================================================
 *  环形缓冲 API
 * ================================================================== */
static inline uint16_t rb_available(void)
{
    if (rb_head >= rb_tail)
        return rb_head - rb_tail;
    else
        return RB_SIZE - rb_tail + rb_head;
}

static inline uint8_t rb_pop(void)
{
    uint8_t ch = rb_buf[rb_tail];
    rb_tail = (rb_tail + 1) % RB_SIZE;
    return ch;
}

/* ==================================================================
 *  ISR 回调：每收到一个字节就塞进环形缓冲
 * ================================================================== */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != &huart3) return;

    uint8_t ch = rb_buf[rb_head];  /* RDR 已由 HAL 读到 rx_buf */
    HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin);

    /* 跳过 \r */
    if (ch != '\r') {
        uint16_t next = (rb_head + 1) % RB_SIZE;
        if (next != rb_tail) {  /* 未满 */
            rb_head = next;
        }
        /* 满了就丢，不阻塞 */
    }

    /* 继续接收下一个字节 */
    HAL_UART_Receive_IT(&huart3, &rb_buf[rb_head], 1);
}

/* ==================================================================
 *  UART 错误 ISR 回调
 * ================================================================== */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART3) return;

    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_ORE))
        __HAL_UART_CLEAR_OREFLAG(huart);
    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_FE))
        __HAL_UART_CLEAR_FEFLAG(huart);

    /* 恢复接收 */
    HAL_UART_Receive_IT(&huart3, &rb_buf[rb_head], 1);
    HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
}

/* ==================================================================
 *  初始化
 * ================================================================== */
void OPENMV_Init(void)
{
    rb_head       = 0;
    rb_tail       = 0;
    it_started    = 0;
    first_byte_tick = 0;
    line_count    = 0;
    last_report   = 0;

    /* 启动中断接收 */
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    HAL_UART_Receive_IT(&huart3, &rb_buf[rb_head], 1);
    it_started = 1;

    printf("[OMV] Init OK (IRQ mode), waiting for OpenMV...\n");
}

/* ==================================================================
 *  主循环调用：从环形缓冲中取字节拼接成行
 * ================================================================== */
void OPENMV_ProcessCommand(void)
{
    static char line_buf[64];
    static uint8_t li = 0;

    while (rb_available() > 0) {
        uint8_t ch = rb_pop();

        if (first_byte_tick == 0) {
            first_byte_tick = HAL_GetTick();
            printf("[OMV] First byte received! (0x%02X)\n", ch);
        }

        if (ch == '\n') {
            line_buf[li] = '\0';
            line_count++;
            ParseCommand(line_buf);
            li = 0;
        } else if (ch != '\r' && li < 63) {
            line_buf[li++] = ch;
        }
    }

    /* 超时清理：5 秒无新指令 → 重置为 NONE */
    extern char last_openmv_cmd[];
    extern uint32_t omv_last_tick;
    if (omv_last_tick > 0 && HAL_GetTick() - omv_last_tick > 5000) {
        strcpy(last_openmv_cmd, "NONE");
        omv_last_tick = 0;
    }

    /* 每 5 秒汇报 */
    if (HAL_GetTick() - last_report > 5000) {
        uint16_t depth = rb_available();
        if (depth > rb_max_depth) rb_max_depth = depth;
        printf("[OMV] alive | lines=%lu buf=%u/%u | tick=%lu\n",
               line_count, depth, rb_max_depth, HAL_GetTick());
        last_report = HAL_GetTick();
    }
}
