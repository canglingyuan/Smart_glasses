#ifndef __SYN6288_H
#define __SYN6288_H

#include "main.h"

extern UART_HandleTypeDef huart2;

void SYN6288_Init(uint32_t baudrate);
void SYN6288_Speak(char *text);

#endif