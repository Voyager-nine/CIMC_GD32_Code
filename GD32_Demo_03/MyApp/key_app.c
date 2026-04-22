#include "key_app.h"

// 使用更明确的变量名
uint8_t g_key_current_state = 0; // 当前按键值 (哪个键按下? 0代表无)
uint8_t g_key_previous_state = 0; // 上一次循环检测到的按键值
uint8_t g_key_pressed_flag = 0; // 按键按下的标志 (哪个键刚被按下?)
uint8_t g_key_released_flag = 0; // 按键释放的标志 (哪个键刚被释放?)

// 读取按键原始状态 (无去抖)
uint8_t key_read_raw(void) {
    uint8_t key_value = 0; // 初始化为0 (无按键)
    // 假设 KEY1-3 在 GPIOB, KEY4 在 GPIOA
    if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_RESET) key_value = 1;  // KEY1
    if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET) key_value = 2;  // KEY2
    if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET) key_value = 3;  // KEY3
    if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_5) == GPIO_PIN_RESET) key_value = 4;  // KEY4
		if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_6) == GPIO_PIN_RESET) key_value = 5;  // KEY5
		if(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) key_value = 6; // KEY6
    return key_value; // 返回当前按下的按键编号 (1-4)，或 0
}

// 极简按键处理函数
void key_process_simple(void) {
    // 1. 读取当前原始状态
    g_key_current_state = key_read_raw();

    // 2. 检测哪些键【刚刚按下】(之前是0/未按，现在是1/按下)
    //    - (g_key_previous_state ^ g_key_current_state) 找出所有状态变化的位
    //    - & g_key_current_state 保留那些当前状态为1 (按下) 的变化位
    g_key_pressed_flag = g_key_current_state & (g_key_previous_state ^ g_key_current_state);

    // 3. 检测哪些键【刚刚释放】(之前是1/按下，现在是0/未按)
    //    - (g_key_previous_state ^ g_key_current_state) 同样找出所有状态变化的位
    //    - & ~g_key_current_state (或 & g_key_previous_state) 保留那些当前状态为0 (释放) 的变化位
    g_key_released_flag = ~g_key_current_state & (g_key_previous_state ^ g_key_current_state);
    // 等效写法: g_key_released_flag = g_key_previous_state & (g_key_previous_state ^ g_key_current_state);

    // 4. 更新上一次的状态，为下一次比较做准备
    g_key_previous_state = g_key_current_state;
	
//		if(g_key_pressed_flag == 1)
//			ucled[0] ^= 1;		
//		if(g_key_pressed_flag == 2)
//			ucled[2] ^= 1;
		
}
