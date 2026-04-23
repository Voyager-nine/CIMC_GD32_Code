#ifndef OLED_APP_H
#define OLED_APP_H

#include "mydefine.h"
void oled_init(void);


void oled_u8g2_init(void);

void OLED_SendBuff(uint8_t buff[4][128]);
void oled_page_init(void);
void oled_task(void);

#endif
