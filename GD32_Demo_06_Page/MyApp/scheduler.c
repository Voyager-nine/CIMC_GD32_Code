#include "scheduler.h"

/*
 * 这是一个最简单的轮询式软调度器。
 *
 * 设计目标：
 * 1. 不使用 RTOS。
 * 2. 通过 HAL_GetTick() 提供的毫秒时基，周期性调用各个后台任务。
 * 3. 将“外设驱动初始化”和“业务逻辑处理”拆开。
 *
 * 当前任务分工：
 * - led_task           : LED 显示刷新
 * - key_process_simple : 基础按键扫描
 * - wave_key_task      : 按键对应的波形控制逻辑
 * - ebtn_task          : 扩展按键库处理
 * - uart_task          : 串口命令解析
 * - adc_task           : ADC 数据分析、打印和回采处理
 */

/* 当前任务表中的任务数量 */
uint8_t task_num;

/**
 * @brief 软件任务控制块
 */
typedef struct
{
    void (*task_func)(void);  /* 任务函数入口 */
    uint32_t rate_ms;         /* 任务执行周期，单位 ms */
    uint32_t last_run;        /* 上一次执行时刻，单位 ms */
} task_t;

/*
 * 调度任务表。
 *
 * 注意：
 * 1. 周期越短的任务，越应该保证执行时间短。
 * 2. 这里所有任务都是主循环中顺序执行，不可阻塞太久。
 * 3. adc_task 和 uart_task 内部如果做大量串口输出，会影响整体实时性。
 */
static task_t scheduler_task[] =
{
    {led_task, 1U, 0U},            /* 1ms 刷新一次 LED 状态 */
    {key_process_simple, 10U, 0U}, /* 10ms 扫描一次基础按键 */
    {wave_key_task, 10U, 0U},      /* 10ms 处理一次波形按键业务 */
    {ebtn_task, 5U, 0U},           /* 5ms 处理一次扩展按键库 */
    {uart_task, 5U, 0U},           /* 5ms 处理一次串口接收和命令 */
    {adc_task, 5U, 0U},             /* 5ms 检查一次 ADC 数据块是否就绪 */
		{oled_task,1,0}
};

void scheduler_init(void)
{
    /* 根据任务表大小自动计算任务数量，避免手工维护 */
    task_num = sizeof(scheduler_task) / sizeof(task_t);
}

void scheduler_run(void)
{
    uint8_t i;

    /*
     * 每进入一次主循环，就顺序检查所有任务是否到了执行时间。
     * 如果“当前时间 - 上次执行时间 >= 任务周期”，就运行该任务。
     */
    for (i = 0U; i < task_num; i++)
    {
        uint32_t now_time = HAL_GetTick();

        if (now_time >= (scheduler_task[i].last_run + scheduler_task[i].rate_ms))
        {
            scheduler_task[i].last_run = now_time;
            scheduler_task[i].task_func();
        }
    }
}
