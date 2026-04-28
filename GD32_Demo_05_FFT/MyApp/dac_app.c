#include "mydefine.h"
#include "dac_app.h"

extern TIM_HandleTypeDef htim6;
extern DAC_HandleTypeDef hdac;

static wave_ctrl_t g_wave_ctrl;
static uint16_t g_dac_wave_table[DAC_TABLE_SIZE];

static float clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

static uint16_t clamp_u16_from_float(float value, uint16_t max_value)
{
    if (value < 0.0f)
    {
        return 0U;
    }

    if (value > (float)max_value)
    {
        return max_value;
    }

    return (uint16_t)(value + 0.5f);
}

static uint32_t wave_calc_tim6_arr(float freq_hz)
{
    float update_rate_hz;
    uint32_t arr_value;

    freq_hz = clamp_float(freq_hz, WAVE_FREQ_MIN_HZ, WAVE_FREQ_MAX_HZ);
    update_rate_hz = freq_hz * (float)g_wave_ctrl.table_size;
    arr_value = (uint32_t)(1000000.0f / update_rate_hz);

    if (arr_value == 0U)
    {
        arr_value = 1U;
    }

    return arr_value;
}

static void dac_wave_restart(void)
{
    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
    HAL_TIM_Base_Stop(&htim6);

    __HAL_TIM_SET_COUNTER(&htim6, 0U);
    HAL_TIM_Base_Start(&htim6);
    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)g_dac_wave_table, g_wave_ctrl.table_size, DAC_ALIGN_12B_R);
}

const wave_ctrl_t *dac_get_ctrl(void)
{
    return &g_wave_ctrl;
}

const char *wave_type_to_string(wave_type_t type)
{
    switch (type)
    {
    case WAVE_TYPE_SINE:
        return "sine";

    case WAVE_TYPE_SQUARE:
        return "square";

    case WAVE_TYPE_TRIANGLE:
        return "triangle";

    default:
        return "unknown";
    }
}

void wave_generate_table(wave_type_t type, uint16_t *table_buf, uint16_t table_size, float vpp)
{
    uint16_t i;
    float amplitude_code;

    if ((table_buf == NULL) || (table_size == 0U))
    {
        return;
    }

    vpp = clamp_float(vpp, WAVE_VPP_MIN_V, WAVE_VPP_MAX_V);
    amplitude_code = (vpp * 0.5f / ADC_REF_VOLTAGE) * DAC_MAX_VALUE;

    for (i = 0U; i < table_size; i++)
    {
        float normalized_value = 0.0f;
        float dac_point_value;

        switch (type)
        {
        case WAVE_TYPE_SINE:
            normalized_value = sinf((2.0f * 3.1415926f * (float)i) / (float)table_size);
            break;

        case WAVE_TYPE_SQUARE:
            normalized_value = (i < (table_size / 2U)) ? 1.0f : -1.0f;
            break;

        case WAVE_TYPE_TRIANGLE:
            if (i < (table_size / 2U))
            {
                normalized_value = -1.0f + (4.0f * (float)i / (float)table_size);
            }
            else
            {
                normalized_value = 3.0f - (4.0f * (float)i / (float)table_size);
            }
            break;

        default:
            normalized_value = 0.0f;
            break;
        }

        dac_point_value = (float)DAC_MID_VALUE + normalized_value * amplitude_code;
        table_buf[i] = clamp_u16_from_float(dac_point_value, DAC_MAX_VALUE);
    }
}

void wave_set_type(wave_type_t type)
{
    g_wave_ctrl.wave_type = type;
    wave_generate_table(g_wave_ctrl.wave_type, g_dac_wave_table, g_wave_ctrl.table_size, g_wave_ctrl.vpp_set);
    dac_wave_restart();
}

void wave_set_frequency(float freq_hz)
{
    uint32_t arr_value;

    freq_hz = clamp_float(freq_hz, WAVE_FREQ_MIN_HZ, WAVE_FREQ_MAX_HZ);
    arr_value = wave_calc_tim6_arr(freq_hz);

    g_wave_ctrl.freq_hz = freq_hz;
    g_wave_ctrl.period_ms = 1000.0f / freq_hz;

    __HAL_TIM_SET_AUTORELOAD(&htim6, arr_value - 1U);
    __HAL_TIM_SET_COUNTER(&htim6, 0U);
}

void wave_set_period(float period_ms)
{
    if (period_ms <= 0.0f)
    {
        return;
    }

    wave_set_frequency(1000.0f / period_ms);
}

void wave_set_vpp(float vpp)
{
    g_wave_ctrl.vpp_set = clamp_float(vpp, WAVE_VPP_MIN_V, WAVE_VPP_MAX_V);
    wave_generate_table(g_wave_ctrl.wave_type, g_dac_wave_table, g_wave_ctrl.table_size, g_wave_ctrl.vpp_set);
    dac_wave_restart();
}

void dac_set_print_enable(uint8_t enable)
{
    g_wave_ctrl.print_enable = (enable != 0U) ? 1U : 0U;
}

void dac_toggle_print_enable(void)
{
    g_wave_ctrl.print_enable ^= 1U;
}

void wave_ctrl_init(void)
{
    memset(&g_wave_ctrl, 0, sizeof(g_wave_ctrl));

    g_wave_ctrl.wave_type = WAVE_TYPE_SINE;
    g_wave_ctrl.freq_hz = 100.0f;
    g_wave_ctrl.period_ms = 10.0f;
    g_wave_ctrl.vpp_set = 1.0f;
    g_wave_ctrl.table_size = DAC_TABLE_SIZE;
    g_wave_ctrl.print_enable = 0U;

    wave_generate_table(g_wave_ctrl.wave_type, g_dac_wave_table, g_wave_ctrl.table_size, g_wave_ctrl.vpp_set);
    wave_set_frequency(g_wave_ctrl.freq_hz);
    dac_wave_restart();
}
