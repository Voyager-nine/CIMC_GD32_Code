#ifndef KEY_APP_H
#define KEY_APP_H

#include "mydefine.h"

/* 当前按下的按键编号，0 表示无按键 */
extern uint8_t g_key_current_state;
/* 上一次扫描到的按键编号 */
extern uint8_t g_key_previous_state;
/* 本次扫描检测到的新按下事件 */
extern uint8_t g_key_pressed_flag;
/* 本次扫描检测到的释放事件 */
extern uint8_t g_key_released_flag;

/**
 * @brief 简单按键扫描任务，负责生成按下/释放事件
 */
void key_process_simple(void);

#endif
