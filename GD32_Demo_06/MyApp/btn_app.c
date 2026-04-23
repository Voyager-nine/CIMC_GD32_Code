#include "btn_app.h"
#include "ebtn.h"

/* 添加全局变量来跟踪组合键状态 */
static volatile uint8_t combo_key_active = 0; // 标记组合键是否激活

/* 1. 定义key_id */
typedef enum
{	
    /* 静态按键id */
    USER_BUTTON_1 = 1,
    USER_BUTTON_2,
    USER_BUTTON_3,
    USER_BUTTON_4,
    USER_BUTTON_5,
    USER_BUTTON_6,
    USER_BUTTON_MAX,

    /* 组合按键id */
    USER_BUTTON_COMBO_1 = 0x100,
    USER_BUTTON_COMBO_2,
    USER_BUTTON_COMBO_3,
    USER_BUTTON_COMBO_MAX,
} user_button_t;

/* 2. 定义按键参数实例 */
// 参数宏: EBTN_PARAMS_INIT(
//     按下消抖时间ms, 释放消抖时间ms,
//     单击有效最短按下时间ms, 单击有效最长按下时间ms,
//     多次单击最大间隔时间ms,
//     长按(KeepAlive)事件周期ms (0禁用),
//     最大连续有效点击次数 (e.g., 1=单击, 2=双击, ...)
// )
const ebtn_btn_param_t key_param_normal = EBTN_PARAMS_INIT(
    20,     // time_debounce: 按下稳定 20ms
    20,     // time_debounce_release: 释放稳定 20ms
    50,     // time_click_pressed_min: 最短单击按下 50ms
    500,    // time_click_pressed_max: 最长单击按下 500ms (超过则不算单击)
    300,    // time_click_multi_max: 多次单击最大间隔 300ms (两次点击间隔超过则重新计数)
    500,    // time_keepalive_period: 长按事件周期 500ms (按下超过 500ms 后，每 500ms 触发一次)
    5       // max_consecutive: 最多支持 5 连击
);

/* 3. 定义静态按键列表 */
// 宏: EBTN_BUTTON_INIT(按键ID, 参数指针)
ebtn_btn_t static_buttons[] = {
    EBTN_BUTTON_INIT(USER_BUTTON_1, &key_param_normal), // KEY1, ID=1, 使用 'key_param_normal' 参数
    EBTN_BUTTON_INIT(USER_BUTTON_2, &key_param_normal), // KEY2, ID=2, 也使用 'key_param_normal' 参数
    EBTN_BUTTON_INIT(USER_BUTTON_3, &key_param_normal), // KEY1, ID=1, 使用 'key_param_normal' 参数
    EBTN_BUTTON_INIT(USER_BUTTON_4, &key_param_normal), // KEY2, ID=2, 也使用 'key_param_normal' 参数
    EBTN_BUTTON_INIT(USER_BUTTON_5, &key_param_normal), // KEY1, ID=1, 使用 'key_param_normal' 参数
    EBTN_BUTTON_INIT(USER_BUTTON_6, &key_param_normal), // KEY2, ID=2, 也使用 'key_param_normal' 参数	
};

/* 注意配置组合按键的按键数组和配置静态按键的按键数组所用的函数是不一样的 */
static ebtn_btn_combo_t btns_combo[] = {
    EBTN_BUTTON_COMBO_INIT(USER_BUTTON_COMBO_1, &key_param_normal),
    EBTN_BUTTON_COMBO_INIT(USER_BUTTON_COMBO_2, &key_param_normal),
    EBTN_BUTTON_COMBO_INIT(USER_BUTTON_COMBO_3, &key_param_normal),    	
};

/* 4. 实现获取按键状态的回调函数 */
// 函数原型: uint8_t (*ebtn_get_state_fn)(struct ebtn_btn *btn);
uint8_t my_get_key_state(struct ebtn_btn *btn) {
    // 根据传入的按钮实例中的 key_id 判断是哪个物理按键
    switch (btn->key_id) {
        case USER_BUTTON_1: // 请求读取 KEY1 的状态
            // 假设 KEY1 接在 PB0，按下为低电平 (返回 1 代表按下)
            return (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_RESET);
        case USER_BUTTON_2: // 请求读取 KEY2 的状态

            return (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET);
        case USER_BUTTON_3: // 请求读取 KEY3 的状态

            return (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET);
        case USER_BUTTON_4: // 请求读取 KEY4 的状态

            return (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_5) == GPIO_PIN_RESET);
        case USER_BUTTON_5: // 请求读取 KEY5 的状态

            return (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_6) == GPIO_PIN_RESET);
        case USER_BUTTON_6: // 请求读取 KEY6 的状态

            return (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET);        
        default:
            // 对于库内部处理组合键等情况，或者未知的 key_id，安全起见返回 0 (未按下)
            return 0;
    }
    // 注意：返回值 1 表示 "活动/按下"，0 表示 "非活动/释放"
}

/* 5. 检查按键物理状态 */
static uint8_t is_button_pressed(uint16_t key_id) {
    switch (key_id) {
        case USER_BUTTON_1: return (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_RESET);
        case USER_BUTTON_2: return (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == GPIO_PIN_RESET);
        case USER_BUTTON_3: return (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_4) == GPIO_PIN_RESET);
        case USER_BUTTON_4: return (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_5) == GPIO_PIN_RESET);
        case USER_BUTTON_5: return (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_6) == GPIO_PIN_RESET);
        case USER_BUTTON_6: return (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET);
        default: return 0;
    }
}

/* 6. 检查是否正在形成组合键 */
static uint8_t is_combo_forming(uint16_t current_key_id) {
    switch (current_key_id) {
        case USER_BUTTON_1:
            return is_button_pressed(USER_BUTTON_2) || is_button_pressed(USER_BUTTON_3);
        case USER_BUTTON_2:
            return is_button_pressed(USER_BUTTON_1) || is_button_pressed(USER_BUTTON_3);
        case USER_BUTTON_3:
            return is_button_pressed(USER_BUTTON_1) || is_button_pressed(USER_BUTTON_2);
        default:
            return 0;
    }
}
 
/* 7. 实现处理按键事件的回调函数 */
// 函数原型: void (*ebtn_evt_fn)(struct ebtn_btn *btn, ebtn_evt_t evt);
void prv_btn_event(struct ebtn_btn *btn, ebtn_evt_t evt)
{
    // 示例：假设 Button 1 按键单击事件对应菜单"上"操作
    if ((btn->key_id == USER_BUTTON_1) && (evt == EBTN_EVT_ONPRESS))
    { 
        // ... (你自己的其他按键处理，例如切换 LED) ...
        // 发送"上"消息给 WouoUI
        WOUOUI_MSG_QUE_SEND(msg_up);
    }
    
    // 示例：假设 Button 2 按键单击事件对应菜单"下"操作
    if ((btn->key_id == USER_BUTTON_2) && (evt == EBTN_EVT_ONPRESS))
    { 
        // ... 
        WOUOUI_MSG_QUE_SEND(msg_down);
    }
    
    // 示例：假设 Button 3 按键单击事件对应菜单"左"操作
    if ((btn->key_id == USER_BUTTON_3) && (evt == EBTN_EVT_ONPRESS))
    { 
        // ... 
        WOUOUI_MSG_QUE_SEND(msg_left);
    }
    
    // 示例：假设 Button 4 按键单击事件对应菜单"右"操作
    if ((btn->key_id == USER_BUTTON_4) && (evt == EBTN_EVT_ONPRESS))
    { 
        // ... 
        WOUOUI_MSG_QUE_SEND(msg_right);
    }
    
    // 示例：假设 Button 5 按键单击事件对应菜单"返回"操作
    if ((btn->key_id == USER_BUTTON_5) && (evt == EBTN_EVT_ONPRESS))
    { 
        // ... 
        WOUOUI_MSG_QUE_SEND(msg_return);
    }
    
    // 示例：假设 Button 6 按键单击事件对应菜单"确认"操作
    if ((btn->key_id == USER_BUTTON_6) && (evt == EBTN_EVT_ONPRESS))
    { 
        // ... 
        WOUOUI_MSG_QUE_SEND(msg_click);
    }
    
    // 可以添加对长按、双击等事件的处理，并映射到 WouoUI 消息
    // if ((btn->key_id == USER_BUTTON_X) && (evt == EBTN_EVT_LONG_PRESS_START)) { ... }
}

// 1. 在初始化找到找到参与组合的普通按键的内部索引 (Index)
// 注意：这个内部索引不一定等于你设置的 key_id！
void my_ebtn_init(void)
{
    ebtn_init(
        static_buttons,                 // 静态按键数组的指针
        EBTN_ARRAY_SIZE(static_buttons), // 静态按键数量 (用宏计算)
        btns_combo,                  // 静态组合按键数组的指针 (如果没有，传 NULL, 0)
        EBTN_ARRAY_SIZE(btns_combo), // 静态组合按键数量 (如果没有，传 0)
        my_get_key_state,               // 你的状态获取回调函数
        prv_btn_event             // 你的事件处理回调函数
    );
    int key1_index = ebtn_get_btn_index_by_key_id(USER_BUTTON_1); // 获取 KEY1 (ID=1) 的内部索引
    int key2_index = ebtn_get_btn_index_by_key_id(USER_BUTTON_2); // 获取 KEY2 (ID=2) 的内部索引
    int key3_index = ebtn_get_btn_index_by_key_id(USER_BUTTON_3); // 获取 KEY3 (ID=3) 的内部索引

    // 2. 将这些索引对应的按键添加到组合键定义中
    //    确保索引有效 (>= 0)
    if (key1_index >= 0 && key2_index >= 0) 
    {
        // 假设 btns_combo[0] 是我们定义的 ID=101 的组合键
        ebtn_combo_btn_add_btn_by_idx(&btns_combo[0], key1_index); // 将 KEY1 添加到组合键
        ebtn_combo_btn_add_btn_by_idx(&btns_combo[0], key2_index); // 将 KEY2 添加到组合键

        ebtn_combo_btn_add_btn_by_idx(&btns_combo[1], key1_index); // 将 KEY1 添加到组合键
        ebtn_combo_btn_add_btn_by_idx(&btns_combo[1], key3_index); // 将 KEY3 添加到组合键

        ebtn_combo_btn_add_btn_by_idx(&btns_combo[2], key2_index); // 将 KEY2 添加到组合键
        ebtn_combo_btn_add_btn_by_idx(&btns_combo[2], key3_index); // 将 KEY3 添加到组合键
    }    
}

void ebtn_task(void)
{
	ebtn_process(HAL_GetTick());
}
