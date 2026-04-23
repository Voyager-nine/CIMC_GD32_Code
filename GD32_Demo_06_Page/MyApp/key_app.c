#include "key_app.h"

/*
 * 这个文件只负责完成“基础按键扫描”和“按键边沿事件提取”。
 *
 * 设计思路：
 * 1. 先读取当前时刻到底是哪个物理按键被按下。
 * 2. 再与上一次扫描结果比较。
 * 3. 由此生成“刚刚按下”和“刚刚释放”两个事件标志。
 *
 * 后续真正的功能分配并不在这里做，而是在 wave_key_task() 中完成，
 * 这样按键扫描层和业务控制层是解耦的。
 */

/* 当前检测到的按键编号，0 表示当前没有按键按下 */
uint8_t g_key_current_state = 0U;
/* 上一次扫描到的按键编号 */
uint8_t g_key_previous_state = 0U;
/* 本次扫描检测到的“新按下”按键编号 */
uint8_t g_key_pressed_flag = 0U;
/* 本次扫描检测到的“刚释放”按键编号 */
uint8_t g_key_released_flag = 0U;

/**
 * @brief 读取按键原始状态
 * @retval 当前被按下的按键编号；如果没有按键按下，则返回 0
 *
 * 说明：
 * 1. 本函数只读取 GPIO 电平，不做消抖。
 * 2. 当前工程假设同一时刻只处理一个按键事件。
 * 3. 如果多个键同时按下，后面判断到的按键会覆盖前面的结果。
 */
static uint8_t key_read_raw(void)
{
    uint8_t key_value = 0U;

    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_RESET) key_value = 1U;  /* KEY1 */
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET) key_value = 2U;  /* KEY2 */
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET) key_value = 3U;  /* KEY3 */
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_5) == GPIO_PIN_RESET) key_value = 4U;  /* KEY4 */
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_6) == GPIO_PIN_RESET) key_value = 5U;  /* KEY5 */
    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) key_value = 6U; /* KEY6 */

    return key_value;
}

/**
 * @brief 基础按键扫描任务
 *
 * 处理流程：
 * 1. 读取当前按键状态。
 * 2. 和上一次扫描结果比较。
 * 3. 生成“按下事件”和“释放事件”。
 * 4. 保存本次状态，供下次扫描使用。
 *
 * 这里用的是“边沿检测”思路：
 * - g_key_pressed_flag 只会在按键从“未按下”变成“按下”那一刻置位一次
 * - g_key_released_flag 只会在按键从“按下”变成“未按下”那一刻置位一次
 */
void key_process_simple(void)
{
    /* 读取当前原始键值 */
    g_key_current_state = key_read_raw();

    /*
     * 检测“刚刚按下”的按键：
     * 只有当前状态和上一次状态不同，并且当前状态非 0，才说明出现了新按下事件。
     */
    g_key_pressed_flag = g_key_current_state & (g_key_previous_state ^ g_key_current_state);

    /*
     * 检测“刚刚释放”的按键：
     * 只有当前状态和上一次状态不同，并且当前状态已经回到 0，才说明出现了释放事件。
     */
    g_key_released_flag = (uint8_t)(~g_key_current_state) & (g_key_previous_state ^ g_key_current_state);

    /* 保存本次状态，供下一次扫描比较使用 */
    g_key_previous_state = g_key_current_state;
}
