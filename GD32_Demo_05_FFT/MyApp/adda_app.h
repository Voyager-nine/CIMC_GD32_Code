#ifndef ADDA_APP_H
#define ADDA_APP_H

#include "mydefine.h"

/* DAC 波形查找表长度，即一个完整周期的离散点数 */
#define DAC_TABLE_SIZE              100U
/* DAC 中点码值，对应输出电压约 1.65V */
#define DAC_MID_VALUE               2048U
/* 12 位 DAC 的满量程码值 */
#define DAC_MAX_VALUE               4095U

/* ADC DMA 原始缓冲区长度，双通道交织存储，必须为 2 的倍数 */
#define ADC_DMA_BUF_LEN             2000U
/* 单个通道可用于分析的有效采样点数 */
#define ADC_ANALYZE_BUF_LEN         (ADC_DMA_BUF_LEN / 2U)

/* ADC / DAC 的参考电压 */
#define ADC_REF_VOLTAGE             3.3f
/* 12 位 ADC 的满量程值 */
#define ADC_FULL_SCALE              4095.0f
/* 波形回采通道的采样率，单位 Hz */
#define ADC_WAVE_SAMPLE_RATE_HZ     10000.0f

/* 允许设置的最小峰峰值，单位 V */
#define WAVE_VPP_MIN_V              0.2f
/* 允许设置的最大峰峰值，单位 V */
#define WAVE_VPP_MAX_V              3.0f
/* 允许设置的最小输出频率，单位 Hz */
#define WAVE_FREQ_MIN_HZ            10.0f
/* 允许设置的最大输出频率，单位 Hz */
#define WAVE_FREQ_MAX_HZ            500.0f

/**
 * @brief 波形类型枚举
 */
typedef enum
{
    WAVE_TYPE_SINE = 0,      /* 正弦波 */
    WAVE_TYPE_SQUARE,        /* 方波 */
    WAVE_TYPE_TRIANGLE       /* 三角波 */
} wave_type_t;

/**
 * @brief 当前输出波形的控制参数
 */
typedef struct
{
    wave_type_t wave_type;   /* 当前输出波形类型 */
    float freq_hz;           /* 当前设定频率，单位 Hz */
    float period_ms;         /* 当前设定周期，单位 ms */
    float vpp_set;           /* 当前设定峰峰值，单位 V */
    uint16_t table_size;     /* 当前波表长度 */
    uint8_t print_enable;    /* 串口打印波形开关，1 为启动 */
} wave_ctrl_t;

/**
 * @brief ADC 采样与分析结果
 */
typedef struct
{
    uint32_t adc_raw_buf[ADC_DMA_BUF_LEN];   /* ADC DMA 原始数据，PC0 与 PA5 交织存放 */
    uint16_t pot_buf[ADC_ANALYZE_BUF_LEN];   /* 从原始数据拆分出的电位器通道数据 */
    uint16_t wave_buf[ADC_ANALYZE_BUF_LEN];  /* 从原始数据拆分出的波形回采通道数据 */
    uint16_t print_buf[ADC_ANALYZE_BUF_LEN]; /* 截取后用于串口打印的数据 */

    uint16_t pot_avg;          /* 电位器通道平均码值 */
    uint16_t wave_max;         /* 波形通道最大码值 */
    uint16_t wave_min;         /* 波形通道最小码值 */
    uint16_t period_samples;   /* 一个周期对应的采样点数 */
    uint16_t print_samples;    /* 当前需要打印的采样点数 */

    float calc_vpp;            /* 通过 ADC 计算得到的峰峰值，单位 V */
    float calc_freq_hz;        /* 通过 ADC 计算得到的频率，单位 Hz */
    float calc_period_ms;      /* 通过 ADC 计算得到的周期，单位 ms */
    wave_type_t calc_type;     /* 通过 ADC 粗识别得到的波形类型 */

    uint8_t adc_block_ready;   /* 一整块 ADC 数据采样完成标志 */
} wave_meas_t;

/* 全局波形控制参数 */
extern wave_ctrl_t g_wave_ctrl;
/* 全局波形测量结果 */
extern wave_meas_t g_wave_meas;

/**
 * @brief 初始化波形控制参数、默认波表和 DAC 输出
 */
void wave_ctrl_init(void);

/**
 * @brief 根据指定波形类型和峰峰值生成一周期波表
 * @param type      波形类型
 * @param table_buf 波表缓冲区指针
 * @param table_size 波表点数
 * @param vpp       峰峰值，单位 V
 */
void wave_generate_table(wave_type_t type, uint16_t *table_buf, uint16_t table_size, float vpp);

/**
 * @brief 设置输出波形类型，并重启 DAC DMA
 * @param type 波形类型
 */
void wave_set_type(wave_type_t type);

/**
 * @brief 设置输出频率，并更新 TIM6 自动重装值
 * @param freq_hz 目标频率，单位 Hz
 */
void wave_set_frequency(float freq_hz);

/**
 * @brief 设置输出周期，内部会换算为频率
 * @param period_ms 目标周期，单位 ms
 */
void wave_set_period(float period_ms);

/**
 * @brief 设置输出峰峰值，并重建波表
 * @param vpp 目标峰峰值，单位 V
 */
void wave_set_vpp(float vpp);

/**
 * @brief 按键控制任务，根据按键事件修改波形参数
 */
void wave_key_task(void);

/**
 * @brief 启动 TIM3 触发的 ADC DMA 采样
 */
void adc_tim_dma_init(void);

/**
 * @brief ADC 后台处理任务，完成数据拆分、分析和可选打印
 */
void adc_task(void);

/**
 * @brief 将 ADC 双通道交织数据拆分为独立通道数据
 */
void adc_split_channels(void);

/**
 * @brief 根据电位器采样结果更新峰峰值设定
 */
void adc_update_pot_value(void);

/**
 * @brief 通过波形采样数据计算峰峰值
 */
void wave_calc_vpp(void);

/**
 * @brief 通过波形采样数据计算频率和周期
 */
void wave_calc_freq(void);

/**
 * @brief 立即等待一块新的 ADC 数据并刷新测量结果
 * @param timeout_ms 等待超时时间，单位 ms
 */
void wave_refresh_measurement_now(uint32_t timeout_ms);

/**
 * @brief 根据采样数据粗略识别波形类型
 * @retval 识别出的波形类型
 */
wave_type_t wave_identify_type(void);

/**
 * @brief 通过串口打印两个周期的 ADC 波形数据
 */
void wave_print_two_periods(void);

/**
 * @brief 将波形类型枚举转换为串口可打印的字符串
 * @param type 波形类型
 * @retval 波形类型字符串
 */
const char *wave_type_to_string(wave_type_t type);

#endif
