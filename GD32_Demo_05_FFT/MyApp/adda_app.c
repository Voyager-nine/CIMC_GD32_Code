#include "adda_app.h"

extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim6;
extern DAC_HandleTypeDef hdac;

extern uint8_t g_key_pressed_flag;

/*
 * 这个文件是本次 AD/DA 任务的核心。
 *
 * 它负责的事情包括：
 * 1. 维护当前波形控制参数（波形类型、频率、周期、峰峰值、打印开关）。
 * 2. 生成 DAC 查找表，并通过 TIM6 + DAC DMA 输出波形。
 * 3. 启动 TIM3 触发的 ADC DMA 采样。
 * 4. 将 ADC 双通道交织数据拆分成“电位器通道”和“波形回采通道”。
 * 5. 根据波形回采数据计算频率、周期、峰峰值，并识别波形类型。
 * 6. 按需通过串口打印两个周期的数据。
 *
 * 外设关系：
 * - PA4 : DAC 输出
 * - PA5 : ADC 回采 DAC 输出波形
 * - PC0 : ADC 采样电位器，用于调峰峰值
 * - TIM6: 作为 DAC 触发源
 * - TIM3: 作为 ADC 触发源
 */

/* 全局波形控制参数对象，保存“设定值” */
wave_ctrl_t g_wave_ctrl;
/* 全局波形测量对象，保存“ADC 测量值” */
wave_meas_t g_wave_meas;

/* DAC 输出使用的一周期查找表 */
static uint16_t g_dac_wave_table[DAC_TABLE_SIZE];

/**
 * @brief 对浮点数做上下限裁剪
 * @param value     原始值
 * @param min_value 最小值
 * @param max_value 最大值
 * @retval 裁剪后的值
 */
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

/**
 * @brief 把浮点数转换为 uint16_t，并在合法范围内裁剪
 * @param value     原始浮点值
 * @param max_value 允许的最大整数值
 * @retval 转换后的整数值
 */
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

/**
 * @brief 启动一轮 ADC DMA 采样
 *
 * 说明：
 * ADC1 当前配置为双通道扫描，因此 DMA 原始缓冲区中的数据形式为：
 * PC0, PA5, PC0, PA5, PC0, PA5, ...
 *
 * 每次启动采样前先清空旧缓冲区，避免观察数据时混入旧值。
 */
static void adc_start_dma_capture(void)
{
    memset(g_wave_meas.adc_raw_buf, 0, sizeof(g_wave_meas.adc_raw_buf));
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_wave_meas.adc_raw_buf, ADC_DMA_BUF_LEN);
    __HAL_DMA_DISABLE_IT(&hdma_adc1, DMA_IT_HT);
}

/**
 * @brief 根据目标频率计算 TIM6 的自动重装值
 * @param freq_hz 目标输出频率，单位 Hz
 * @retval TIM6 对应的周期计数值
 *
 * 计算依据：
 * 1. 当前工程假设 TIM6 计数时钟经预分频后为 1MHz。
 * 2. 一个完整波形周期包含 g_wave_ctrl.table_size 个点。
 * 3. 因此 TIM6 更新率 = 波形频率 * 波表点数。
 */
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

/**
 * @brief 找到当前采样块中第一个“中线向上穿越点”
 * @retval 穿越点索引；如果没有找到则返回 0
 *
 * 解释：
 * 这里的“中线”是 (最大值 + 最小值) / 2。
 * 当波形从低于中线变成高于等于中线，就认为出现一次上升穿越。
 */
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

/**
 * @brief 重新启动 DAC DMA 输出，使新波表参数立即生效
 *
 * 说明：
 * 当波形类型或峰峰值改变后，需要重建波表并重新启动 DAC DMA，
 * 否则硬件仍会按旧波表循环输出。
 */
static void dac_wave_restart(void)
{
    HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
    HAL_TIM_Base_Stop(&htim6);

    __HAL_TIM_SET_COUNTER(&htim6, 0U);
    HAL_TIM_Base_Start(&htim6);
    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)g_dac_wave_table, g_wave_ctrl.table_size, DAC_ALIGN_12B_R);
}

/**
 * @brief 根据 ADC 数据估算一个周期对应的采样点数
 * @retval 周期对应的采样点数；失败返回 0
 *
 * 算法：
 * 1. 找到第一个中线向上穿越点。
 * 2. 再找下一个中线向上穿越点。
 * 3. 两个点之间的样本数就是一个周期的样本数。
 */
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

/**
 * @brief 基于模板匹配识别波形类型
 * @retval 识别结果：正弦 / 方波 / 三角波
 *
 * 设计原因：
 * 早期用“平台个数”和“差分稳定性”做粗判，正弦和三角之间容易误判。
 * 当前算法改为：
 * 1. 截取一个周期数据。
 * 2. 做归一化。
 * 3. 与理想正弦 / 方波 / 三角模板比较误差。
 * 4. 对一个周期内所有可能相位都搜索一次。
 * 5. 误差最小的模板，就是最终识别结果。
 *
 * 这样三角波和正弦波的识别稳定性会更好。
 */
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

/**
 * @brief 对当前 ADC 缓冲区做一次完整分析
 *
 * 包括：
 * 1. 拆分双通道数据
 * 2. 更新旋钮设定值
 * 3. 计算峰峰值
 * 4. 计算频率和周期
 * 5. 识别波形类型
 */
static void wave_analyze_current_buffer(void)
{
    adc_split_channels();
    adc_update_pot_value();
    wave_calc_vpp();
    wave_calc_freq();
    g_wave_meas.calc_type = wave_identify_type();
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
    /* 波表索引，用于遍历一个周期内的每个采样点。 */
    uint16_t i;
    /* 峰值幅度对应的 DAC 码值（以中点为中心的半幅）。 */
    float amplitude_code;

    /* 防御式检查：缓冲区为空或长度为 0 时直接返回，避免非法访问。 */
    if ((table_buf == NULL) || (table_size == 0U))
    {
        /* 参数无效，不执行后续计算。 */
        return;
    }

    /*
     * 峰峰值以“电压值”输入，内部先换算成 DAC 码值幅度。
     * 最终输出波形都是以 DAC_MID_VALUE 为中心上下摆动。
     */
    /* 将输入峰峰值限制在允许范围，避免超出硬件或算法设计边界。 */
    vpp = clamp_float(vpp, WAVE_VPP_MIN_V, WAVE_VPP_MAX_V);
    /* 把电压幅度换算为 DAC 码值幅度：Vpp/2 -> 码值半幅。 */
    amplitude_code = (vpp * 0.5f / ADC_REF_VOLTAGE) * DAC_MAX_VALUE;

    /* 逐点生成一个周期的波形查找表。 */
    for (i = 0U; i < table_size; i++)
    {
        /* 归一化波形值，目标范围约为 [-1, 1]。 */
        float normalized_value = 0.0f;
        /* 当前点最终对应的 DAC 码值（浮点中间量）。 */
        float dac_point_value;

        /* 根据目标波形类型，计算当前采样点的归一化幅值。 */
        switch (type)
        {
        case WAVE_TYPE_SINE:
            /* 正弦波：按当前索引映射相位并取 sin。 */
            normalized_value = sinf((2.0f * 3.1415926f * (float)i) / (float)table_size);
            /* 当前点计算完成，退出 switch。 */
            break;

        case WAVE_TYPE_SQUARE:
            /* 方波：前半周期为高电平，后半周期为低电平。 */
            normalized_value = (i < (table_size / 2U)) ? 1.0f : -1.0f;
            /* 当前点计算完成，退出 switch。 */
            break;

        case WAVE_TYPE_TRIANGLE:
            /* 三角波前半周期线性上升：-1 -> +1。 */
            if (i < (table_size / 2U))
            {
                normalized_value = -1.0f + (4.0f * (float)i / (float)table_size);
            }
            else
            {
                /* 三角波后半周期线性下降：+1 -> -1。 */
                normalized_value = 3.0f - (4.0f * (float)i / (float)table_size);
            }
            /* 当前点计算完成，退出 switch。 */
            break;

        default:
            /* 未知波形类型：输出中线（归一化 0）。 */
            normalized_value = 0.0f;
            /* 当前点计算完成，退出 switch。 */
            break;
        }

        /* 以 DAC 中点为基准叠加幅值，得到该点的目标 DAC 码值。 */
        dac_point_value = (float)DAC_MID_VALUE + normalized_value * amplitude_code;
        /* 对码值做范围裁剪并取整后写入波表。 */
        table_buf[i] = clamp_u16_from_float(dac_point_value, DAC_MAX_VALUE);
    }
}

void wave_set_type(wave_type_t type)
{
    /* 更新软件设定值，重建波表，立刻重启 DAC 输出 */
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

    /*
     * 修改 TIM6 自动重装值后，DAC 触发速率就会改变。
     * 因为一个周期仍然输出固定数量的查找表点，所以最终波形频率也跟着变化。
     */
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
    /* 只要峰峰值变化，就必须重建整个波表 */
    g_wave_ctrl.vpp_set = clamp_float(vpp, WAVE_VPP_MIN_V, WAVE_VPP_MAX_V);
    wave_generate_table(g_wave_ctrl.wave_type, g_dac_wave_table, g_wave_ctrl.table_size, g_wave_ctrl.vpp_set);
    dac_wave_restart();
}

void wave_ctrl_init(void)
{
    /*
     * 上电默认参数：
     * - 正弦波
     * - 100Hz
     * - 10ms 周期
     * - 1.0Vpp
     * - 不自动打印波形数据
     */
    memset(&g_wave_ctrl, 0, sizeof(g_wave_ctrl));
    memset(&g_wave_meas, 0, sizeof(g_wave_meas));

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

void adc_tim_dma_init(void)
{
    /*
     * 启动 ADC DMA 后，再启动 TIM3。
     * 后续 TIM3 每来一次触发，ADC 就会按配置顺序采一次 PC0 和 PA5。
     */
    adc_start_dma_capture();
    HAL_TIM_Base_Start(&htim3);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    /*
     * 这里只做一件事：告诉后台“这一整块数据采完了”。
     * 真正的数据分析放到 adc_task() 里做，避免中断回调过重。
     */
    if (hadc->Instance == ADC1)
    {
        HAL_ADC_Stop_DMA(hadc);
        g_wave_meas.adc_block_ready = 1U;
    }
}

void adc_split_channels(void)
{
    uint16_t i;

    /*
     * 原始缓冲区是交织数据：
     * adc_raw_buf[0] = PC0
     * adc_raw_buf[1] = PA5
     * adc_raw_buf[2] = PC0
     * adc_raw_buf[3] = PA5
     *
     * 拆分后：
     * - pot_buf  只保留 PC0
     * - wave_buf 只保留 PA5
     */
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

    /* 先对电位器通道做平均，降低抖动 */
    for (i = 0U; i < ADC_ANALYZE_BUF_LEN; i++)
    {
        sum += g_wave_meas.pot_buf[i];
    }

    g_wave_meas.pot_avg = (uint16_t)(sum / ADC_ANALYZE_BUF_LEN);
    vpp_from_pot = WAVE_VPP_MIN_V +
                   ((float)g_wave_meas.pot_avg / ADC_FULL_SCALE) * (WAVE_VPP_MAX_V - WAVE_VPP_MIN_V);

    /*
     * 只有电位器变化超过一定门限时才刷新波表，
     * 否则会因为 ADC 抖动导致 DAC 输出频繁重建。
     */
    if (fabsf(vpp_from_pot - g_wave_ctrl.vpp_set) > 0.05f)
    {
        wave_set_vpp(vpp_from_pot);
    }
}

void wave_calc_vpp(void)
{
    uint16_t i;
    uint16_t max_value = 0U;
    uint16_t min_value = DAC_MAX_VALUE;

    /* 峰峰值就是回采波形的最大值和最小值之差 */
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
    /*
     * 频率不允许直接拿定时器配置得出，而是必须通过 ADC 数据反算。
     * 所以这里先找出一个周期对应的样本数，再结合 ADC 采样率换算出频率。
     */
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

    /*
     * 只打印两个周期，而不是整块缓冲区，
     * 这样既满足作业要求，又能控制串口输出量。
     */
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

    /*
     * 这个函数主要给串口 set 命令使用。
     * 当用户刚修改完波形、频率、峰峰值后，
     * 我们希望立刻得到一块“新参数下”的 ADC 数据，
     * 这样返回的 [adc] 就不会还是旧值。
     */
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

    /* 无论是否成功取到一块数据，都恢复正常后台采样流程 */
    adc_start_dma_capture();
}

void wave_key_task(void)
{
    /*
     * 按键和功能的对应关系：
     * KEY1 : 频率减 10Hz
     * KEY2 : 频率加 10Hz
     * KEY3 : 波形切换
     * KEY4 : 打印开关
     * KEY5 : 打印当前状态
     * KEY6 : 恢复默认参数
     */
    switch (g_key_pressed_flag)
    {
    case 1U:
        wave_set_frequency(g_wave_ctrl.freq_hz - 10.0f);
        break;

    case 2U:
        wave_set_frequency(g_wave_ctrl.freq_hz + 10.0f);
        break;

    case 3U:
        if (g_wave_ctrl.wave_type == WAVE_TYPE_TRIANGLE)
        {
            wave_set_type(WAVE_TYPE_SINE);
        }
        else
        {
            wave_set_type((wave_type_t)(g_wave_ctrl.wave_type + 1));
        }
        break;

    case 4U:
        g_wave_ctrl.print_enable ^= 1U;
        break;

    case 5U:
        uart_print_status();
        break;

    case 6U:
        wave_set_type(WAVE_TYPE_SINE);
        wave_set_frequency(100.0f);
        wave_set_vpp(1.0f);
        g_wave_ctrl.print_enable = 0U;
        break;

    default:
        break;
    }

    /* 消费掉这次按键事件，避免重复处理 */
    g_key_pressed_flag = 0U;
}

void adc_task(void)
{
    /*
     * 这是 ADC 后台任务的主入口。
     * 只有当一整块 DMA 数据已经采满时，才进行分析。
     */
    if (g_wave_meas.adc_block_ready == 0U)
    {
        return;
    }

    wave_analyze_current_buffer();

    if (g_wave_ctrl.print_enable != 0U)
    {
        wave_print_two_periods();
    }

    g_wave_meas.adc_block_ready = 0U;
    adc_start_dma_capture();
}
