#ifndef __OPENMV_RECEIVER_H
#define __OPENMV_RECEIVER_H

#include "main.h"

#define OPENMV_RX_BUF_SIZE    64          // 接收缓冲区大小
#define OPENMV_CMD_DELIMITER  '\n'         // 指令结束符

void OPENMV_Init(void);
void OPENMV_ProcessCommand(void);
void OPENMV_UART_Callback(uint8_t ch);

#endif