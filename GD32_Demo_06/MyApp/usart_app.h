#ifndef USART_APP_H
#define USART_APP_H

#include "mydefine.h"
#include "usart.h" //my_printf()需要用到该头文件
int my_printf(UART_HandleTypeDef *huart, const char *format, ...);
void uart_task(void);

//void buffer_init(void);
void ringbuffer_init(void);

#endif
