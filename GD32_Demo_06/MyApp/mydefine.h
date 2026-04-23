#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "main.h"
#include "scheduler.h"
#include "led_app.h"
#include "key_app.h"
#include "btn_app.h"
#include "usart_app.h"
#include "adda_app.h"
#include "oled.h"
#include "oled_app.h"
#include "u8g2.h"
#include "i2c.h" 
#include "WouoUI.h"      // WouoUI 核心框架
#include "WouoUI_user.h" // 用户自定义的菜单结构和回调函数 (通常需要用户创建或修改)

extern uint8_t ucled[6];  // LED 状态数组 (6个LED)
extern void OLED_SendBuff(uint8_t buff[4][128]);

