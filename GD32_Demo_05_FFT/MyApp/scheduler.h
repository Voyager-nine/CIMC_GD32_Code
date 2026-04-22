#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "mydefine.h"

/**
 * @brief 初始化轮询调度器
 */
void scheduler_init(void);

/**
 * @brief 执行一轮调度检查，运行到期任务
 */
void scheduler_run(void);

#endif
