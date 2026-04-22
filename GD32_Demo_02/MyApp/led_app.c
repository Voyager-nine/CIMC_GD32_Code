#include "led_app.h"

// PWM周期，单位ms。10ms -> 100Hz PWM频率
#define PWM_PERIOD_MS 10 
// 亮度更新间隔，单位ms。每隔30ms改变一次亮度 
#define BRIGHTNESS_UPDATE_INTERVAL_MS 30
// 查找表步数
#define TABLE_SIZE 100 

// --- 新增：管理多个LED ---
#define NUM_LEDS 6 // 定义LED的数量，方便以后修改

#define PHASE_OFFSET (TABLE_SIZE / NUM_LEDS)

uint8_t ucled[6] = {0,0,0,0,0,0 };  // LED 状态数组 (6个LED)

// 新的查找表，最大值为 PWM_PERIOD_MS (10)
const uint8_t breath_table[TABLE_SIZE] = {
    1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 
    8, 8, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 
    9, 9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 
    2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

/**
 * @brief 根据ucLed数组状态更新6个LED的显示
 * @param ucLed Led数据储存数组 (大小为6)
 */
void led_disp(uint8_t *ucLed)
{
    uint8_t temp = 0x00;                // 用于记录当前 LED 状态的临时变量 (最低6位有效)
    static uint8_t temp_old = 0xff;     // 记录之前 LED 状态的变量, 用于判断是否需要更新显示

    for (int i = 0; i < 6; i++)         // 遍历6个LED的状态
    {
        // 将LED状态整合到temp变量中，方便后续比较
        if (ucLed[i]) temp |= (1 << i); // 如果ucLed[i]为1, 则将temp的第i位置1
    }

    // 仅当当前状态与之前状态不同的时候，才更新显示
    if (temp != temp_old)
    {
        // 使用HAL库函数根据temp的值设置对应引脚状态 (假设高电平点亮)
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, (temp & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 0 (PB6)
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, (temp & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 1 (PB7)
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, (temp & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 2 (PB8)
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, (temp & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 3 (PB9)
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0,  (temp & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 4 (PE0)
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1,  (temp & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET); // LED 5 (PE1)

        temp_old = temp;                // 更新记录的旧状态
    }
}

/**
 * @brief LED 显示处理函数 (主循环调用)
 */
void led_task(void)
{	
		// 只需要一个基准索引，代表流水灯“波头”的位置
		static uint16_t base_table_index = 0; 

		static uint16_t pwm_counter = 0;         // PWM周期内部计数器 (0-9)
		static uint16_t brightness_update_counter = 0; // 亮度更新计数器 (0-29)
	
    // --- 慢速尺度：更新基准索引 ---
    brightness_update_counter++;
    if (brightness_update_counter >= BRIGHTNESS_UPDATE_INTERVAL_MS) {
        brightness_update_counter = 0;

        // 只更新基准索引，让整个“波形”向前移动一步
        base_table_index++;
        if (base_table_index >= TABLE_SIZE) {
            base_table_index = 0;
        }
    }

    // --- 快速尺度：循环处理所有LED的PWM输出 ---
    // 这个循环是新增的核心
    for (int i = 0; i < NUM_LEDS; i++)
    {
        // 1. 计算当前LED在查找表中的实际位置
        //    (基准位置 + 相位偏移) % 表大小，%是为了防止索引越界，实现循环
        uint16_t current_led_index = (base_table_index - i * PHASE_OFFSET + TABLE_SIZE) % TABLE_SIZE;//在0-99之间循环

        // 2. 从查找表中获取该LED此时应有的亮度（占空比）
        uint8_t duty_cycle = breath_table[current_led_index];

        // 3. 根据占空比和PWM计数器，决定是点亮还是熄灭LED
        if (pwm_counter < duty_cycle) 
            ucled[i] = 1;
				else 
            ucled[i] = 0;
    }
    
    // 更新PWM内部计数器，为下一个1ms做准备
    pwm_counter++;
    if (pwm_counter >= PWM_PERIOD_MS) {
        pwm_counter = 0;
    }
		
    led_disp(ucled);                    // 调用led_disp函数更新LED状态
}

