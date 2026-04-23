#ifndef DAC_APP_H
#define DAC_APP_H

#include <stdint.h>

/* DAC one-cycle waveform table size */
#define DAC_TABLE_SIZE              100U
/* Midpoint code for 12-bit DAC output */
#define DAC_MID_VALUE               2048U
/* Full-scale code for 12-bit DAC */
#define DAC_MAX_VALUE               4095U

/* Shared analog reference voltage */
#define ADC_REF_VOLTAGE             3.3f

/* Allowed Vpp range */
#define WAVE_VPP_MIN_V              0.2f
#define WAVE_VPP_MAX_V              3.0f
/* Allowed frequency range */
#define WAVE_FREQ_MIN_HZ            10.0f
#define WAVE_FREQ_MAX_HZ            500.0f

typedef enum
{
    WAVE_TYPE_SINE = 0,
    WAVE_TYPE_SQUARE,
    WAVE_TYPE_TRIANGLE
} wave_type_t;

typedef struct
{
    wave_type_t wave_type;
    float freq_hz;
    float period_ms;
    float vpp_set;
    uint16_t table_size;
    uint8_t print_enable;
} wave_ctrl_t;

void wave_ctrl_init(void);
void wave_generate_table(wave_type_t type, uint16_t *table_buf, uint16_t table_size, float vpp);
void wave_set_type(wave_type_t type);
void wave_set_frequency(float freq_hz);
void wave_set_period(float period_ms);
void wave_set_vpp(float vpp);
void dac_set_print_enable(uint8_t enable);
void dac_toggle_print_enable(void);
const wave_ctrl_t *dac_get_ctrl(void);
const char *wave_type_to_string(wave_type_t type);

#endif
