#ifndef ADDA_APP_H
#define ADDA_APP_H

#include "mydefine.h"
//void adc_read_by_polling(void);

//void adc_dma_init(void);
//void adc_task(void);

void adc_task(void);
void adc_tim_dma_init(void);
void dac_sin_init(void);

#endif
