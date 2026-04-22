#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "main.h"
#include "scheduler.h"
#include "led_app.h"
#include "key_app.h"
#include "btn_app.h"
#include "usart.h"
#include "usart_app.h"

extern uint8_t ucled[6];  // LED ×´Ì¬Êý×é (6¸öLED)

extern DMA_HandleTypeDef hdma_usart1_rx;
