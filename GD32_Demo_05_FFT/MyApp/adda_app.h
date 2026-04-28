#ifndef ADDA_APP_H
#define ADDA_APP_H

#include "mydefine.h"
#include "dac_app.h"

/* ADC DMA raw buffer length for interleaved dual-channel samples */
#define ADC_DMA_BUF_LEN             2000U
/* Per-channel sample count available for analysis */
#define ADC_ANALYZE_BUF_LEN         (ADC_DMA_BUF_LEN / 2U)

/* Full-scale code for 12-bit ADC */
#define ADC_FULL_SCALE              4095.0f
/* ADC sample rate on the waveform feedback channel */
#define ADC_WAVE_SAMPLE_RATE_HZ     10000.0f

typedef struct
{
    uint32_t adc_raw_buf[ADC_DMA_BUF_LEN];
    uint16_t pot_buf[ADC_ANALYZE_BUF_LEN];
    uint16_t wave_buf[ADC_ANALYZE_BUF_LEN];
    uint16_t print_buf[ADC_ANALYZE_BUF_LEN];

    uint16_t pot_avg;
    uint16_t wave_max;
    uint16_t wave_min;
    uint16_t period_samples;
    uint16_t print_samples;

    float calc_vpp;
    float calc_freq_hz;
    float calc_period_ms;
    wave_type_t calc_type;

    uint8_t adc_block_ready;
} wave_meas_t;

extern wave_meas_t g_wave_meas;

void adc_tim_dma_init(void);
void adc_task(void);
void adc_split_channels(void);
void adc_update_pot_value(void);
void wave_calc_vpp(void);
void wave_calc_freq(void);
void wave_refresh_measurement_now(uint32_t timeout_ms);
wave_type_t wave_identify_type(void);
void wave_print_two_periods(void);

#endif
