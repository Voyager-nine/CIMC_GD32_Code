#ifndef MYDEFINE_H
#define MYDEFINE_H

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
#include "dac_app.h"
#include "adda_app.h"
#include "oled.h"
#include "oled_app.h"
#include "u8g2.h"
#include "i2c.h"
#include "WouoUI.h"
#include "WouoUI_user.h"

extern uint8_t ucled[6];
extern void OLED_SendBuff(uint8_t buff[4][128]);

#endif
