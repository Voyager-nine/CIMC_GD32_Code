#ifndef USART_APP_H
#define USART_APP_H

#include "mydefine.h"
#include "usart.h"

/**
 * @brief 串口格式化打印函数
 * @param huart  串口句柄
 * @param format 格式化字符串
 * @retval 实际发送的字符数
 */
int my_printf(UART_HandleTypeDef *huart, const char *format, ...);

/**
 * @brief 串口后台任务，从环形缓冲区取出数据并拼接命令行
 */
void uart_task(void);

/**
 * @brief 解析一整行串口命令
 * @param cmd_line 命令行字符串
 */
void uart_cmd_parse(char *cmd_line);

/**
 * @brief 打印当前设置值和 ADC 计算值
 */
void uart_print_status(void);

/**
 * @brief 初始化串口 DMA + 空闲中断 + 环形缓冲区接收
 */
void ringbuffer_init(void);

#endif
