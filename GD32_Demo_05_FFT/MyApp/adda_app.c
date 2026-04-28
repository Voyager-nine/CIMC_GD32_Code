#include "adda_app.h"

extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim3;

wave_meas_t g_wave_meas;

static void adc_start_dma_capture(void)
{
    memset(g_wave_meas.adc_raw_buf, 0, sizeof(g_wave_meas.adc_raw_buf));
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_wave_meas.adc_raw_buf, ADC_DMA_BUF_LEN);
    __HAL_DMA_DISABLE_IT(&hdma_adc1, DMA_IT_HT);
}

static uint16_t wave_find_first_cross(void)
{
    uint16_t i;
    uint16_t mid_value;

    mid_value = (uint16_t)(((uint32_t)g_wave_meas.wave_max + (uint32_t)g_wave_meas.wave_min) / 2U);

    for (i = 1U; i < ADC_ANALYZE_BUF_LEN; i++)
    {
        if ((g_wave_meas.wave_buf[i - 1U] < mid_value) && (g_wave_meas.wave_buf[i] >= mid_value))
        {
            return i;
        }
    }

    return 0U;
}

static uint16_t wave_find_period_samples(void)
{
    uint16_t i;
    uint16_t first_cross;
    uint16_t second_cross = 0U;
    uint16_t mid_value;

    first_cross = wave_find_first_cross();
    if (first_cross == 0U)
    {
        return 0U;
    }

    mid_value = (uint16_t)(((uint32_t)g_wave_meas.wave_max + (uint32_t)g_wave_meas.wave_min) / 2U);

    for (i = (uint16_t)(first_cross + 1U); i < ADC_ANALYZE_BUF_LEN; i++)
    {
        if ((g_wave_meas.wave_buf[i - 1U] < mid_value) && (g_wave_meas.wave_buf[i] >= mid_value))
        {
            second_cross = i;
            break;
        }
    }

    if ((first_cross == 0U) || (second_cross == 0U) || (second_cross <= first_cross))
    {
        return 0U;
    }

    return (uint16_t)(second_cross - first_cross);
}

static wave_type_t wave_identify_by_template(void)
{
    uint16_t start_index;
    uint16_t i;
    uint16_t phase_offset;
    float amp;
    float mid;
    float best_sine_error = 1.0e30f;
    float best_square_error = 1.0e30f;
    float best_triangle_error = 1.0e30f;

    if (g_wave_meas.period_samples < 8U)
    {
        return WAVE_TYPE_SINE;
    }

    start_index = wave_find_first_cross();
    if ((start_index == 0U) ||
        ((uint32_t)start_index + (uint32_t)g_wave_meas.period_samples > (uint32_t)ADC_ANALYZE_BUF_LEN))
    {
        start_index = 0U;
    }

    mid = ((float)g_wave_meas.wave_max + (float)g_wave_meas.wave_min) * 0.5f;
    amp = ((float)g_wave_meas.wave_max - (float)g_wave_meas.wave_min) * 0.5f;
    if (amp < 1.0f)
    {
        return WAVE_TYPE_SINE;
    }

    for (phase_offset = 0U; phase_offset < g_wave_meas.period_samples; phase_offset++)
    {
        float sine_error = 0.0f;
        float square_error = 0.0f;
        float triangle_error = 0.0f;

        for (i = 0U; i < g_wave_meas.period_samples; i++)
        {
            float phase;
            float sample_norm;
            float sine_ref;
            float square_ref;
            float triangle_ref;
            uint16_t sample_index;

            phase = (float)i / (float)g_wave_meas.period_samples;
            sample_index = start_index + ((i + phase_offset) % g_wave_meas.period_samples);
            sample_norm = ((float)g_wave_meas.wave_buf[sample_index] - mid) / amp;

            sine_ref = sinf(2.0f * 3.1415926f * phase);
            square_ref = (phase < 0.5f) ? 1.0f : -1.0f;
            if (phase < 0.5f)
            {
                triangle_ref = -1.0f + 4.0f * phase;
            }
            else
            {
                triangle_ref = 3.0f - 4.0f * phase;
            }

            sine_error += fabsf(sample_norm - sine_ref);
            square_error += fabsf(sample_norm - square_ref);
            triangle_error += fabsf(sample_norm - triangle_ref);
        }

        if (sine_error < best_sine_error)
        {
            best_sine_error = sine_error;
        }

        if (square_error < best_square_error)
        {
            best_square_error = square_error;
        }

        if (triangle_error < best_triangle_error)
        {
            best_triangle_error = triangle_error;
        }
    }

    if ((best_square_error < best_sine_error) && (best_square_error < best_triangle_error))
    {
        return WAVE_TYPE_SQUARE;
    }

    if ((best_triangle_error < best_sine_error) && (best_triangle_error < best_square_error))
    {
        return WAVE_TYPE_TRIANGLE;
    }

    return WAVE_TYPE_SINE;
}

static void wave_analyze_current_buffer(void)
{
    adc_split_channels();
    adc_update_pot_value();
    wave_calc_vpp();
    wave_calc_freq();
    g_wave_meas.calc_type = wave_identify_type();
}

void adc_tim_dma_init(void)
{
    adc_start_dma_capture();
    HAL_TIM_Base_Start(&htim3);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        HAL_ADC_Stop_DMA(hadc);
        g_wave_meas.adc_block_ready = 1U;
    }
}

void adc_split_channels(void)
{
    uint16_t i;

    for (i = 0U; i < ADC_ANALYZE_BUF_LEN; i++)
    {
        g_wave_meas.pot_buf[i] = (uint16_t)g_wave_meas.adc_raw_buf[i * 2U];
        g_wave_meas.wave_buf[i] = (uint16_t)g_wave_meas.adc_raw_buf[i * 2U + 1U];
    }
}

void adc_update_pot_value(void)
{
    uint32_t sum = 0U;
    uint16_t i;
    float vpp_from_pot;
    const wave_ctrl_t *ctrl = dac_get_ctrl();

    for (i = 0U; i < ADC_ANALYZE_BUF_LEN; i++)
    {
        sum += g_wave_meas.pot_buf[i];
    }

    g_wave_meas.pot_avg = (uint16_t)(sum / ADC_ANALYZE_BUF_LEN);
    vpp_from_pot = WAVE_VPP_MIN_V +
                   ((float)g_wave_meas.pot_avg / ADC_FULL_SCALE) * (WAVE_VPP_MAX_V - WAVE_VPP_MIN_V);

    if ((ctrl != NULL) && (fabsf(vpp_from_pot - ctrl->vpp_set) > 0.05f))
    {
        wave_set_vpp(vpp_from_pot);
    }
}

void wave_calc_vpp(void)
{
    uint16_t i;
    uint16_t max_value = 0U;
    uint16_t min_value = DAC_MAX_VALUE;

    for (i = 0U; i < ADC_ANALYZE_BUF_LEN; i++)
    {
        if (g_wave_meas.wave_buf[i] > max_value)
        {
            max_value = g_wave_meas.wave_buf[i];
        }

        if (g_wave_meas.wave_buf[i] < min_value)
        {
            min_value = g_wave_meas.wave_buf[i];
        }
    }

    g_wave_meas.wave_max = max_value;
    g_wave_meas.wave_min = min_value;
    g_wave_meas.calc_vpp = ((float)(max_value - min_value) * ADC_REF_VOLTAGE) / ADC_FULL_SCALE;
}

void wave_calc_freq(void)
{
    g_wave_meas.period_samples = wave_find_period_samples();

    if (g_wave_meas.period_samples == 0U)
    {
        g_wave_meas.calc_freq_hz = 0.0f;
        g_wave_meas.calc_period_ms = 0.0f;
        return;
    }

    g_wave_meas.calc_freq_hz = ADC_WAVE_SAMPLE_RATE_HZ / (float)g_wave_meas.period_samples;
    g_wave_meas.calc_period_ms = 1000.0f / g_wave_meas.calc_freq_hz;
}

void wave_print_two_periods(void)
{
    uint16_t i;
    uint16_t target_samples;

    if (g_wave_meas.period_samples == 0U)
    {
        my_printf(&huart1, "[wave] period_samples=0, skip print\r\n");
        return;
    }

    target_samples = (uint16_t)(g_wave_meas.period_samples * 2U);
    if (target_samples > ADC_ANALYZE_BUF_LEN)
    {
        target_samples = ADC_ANALYZE_BUF_LEN;
    }

    g_wave_meas.print_samples = target_samples;
    memcpy(g_wave_meas.print_buf, g_wave_meas.wave_buf, target_samples * sizeof(uint16_t));

    my_printf(&huart1, "[wave] print %u samples (%u periods)\r\n", target_samples, 2U);
    for (i = 0U; i < target_samples; i++)
    {
        my_printf(&huart1, "<adc>:%u\r\n", g_wave_meas.print_buf[i]);
    }
}

wave_type_t wave_identify_type(void)
{
    return wave_identify_by_template();
}

void wave_refresh_measurement_now(uint32_t timeout_ms)
{
    uint32_t start_tick;

    g_wave_meas.adc_block_ready = 0U;
    HAL_ADC_Stop_DMA(&hadc1);
    adc_start_dma_capture();

    start_tick = HAL_GetTick();
    while (g_wave_meas.adc_block_ready == 0U)
    {
        if ((HAL_GetTick() - start_tick) >= timeout_ms)
        {
            break;
        }
    }

    if (g_wave_meas.adc_block_ready != 0U)
    {
        wave_analyze_current_buffer();
        g_wave_meas.adc_block_ready = 0U;
    }

    adc_start_dma_capture();
}

void adc_task(void)
{
    const wave_ctrl_t *ctrl = dac_get_ctrl();

    if (g_wave_meas.adc_block_ready == 0U)
    {
        return;
    }

    wave_analyze_current_buffer();

    if ((ctrl != NULL) && (ctrl->print_enable != 0U))
    {
        wave_print_two_periods();
    }

    g_wave_meas.adc_block_ready = 0U;
    adc_start_dma_capture();
}
