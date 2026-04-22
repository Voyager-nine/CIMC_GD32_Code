#include "adda_app.h"


/* ADC轮询模式 */
//__IO uint32_t adc_val; //模拟电压数字量
//__IO float voltage;    //模拟电压转化为数字电压
//extern ADC_HandleTypeDef hadc1;

//// 在需要读取 ADC 的地方调用，比如一个任务函数内
//void adc_read_by_polling(void) 
//{
//    // 1. 启动 ADC 转换
//    HAL_ADC_Start(&hadc1); // hadc1 是你的 ADC 句柄

//    // 2. 等待转换完成 (阻塞式)
//    //    参数 1000 表示超时时间 (毫秒)
//    if (HAL_ADC_PollForConversion(&hadc1, 1000) == HAL_OK) 
//    {
//        // 3. 转换成功，读取数字结果 (0-4095 for 12-bit)
//        adc_val = HAL_ADC_GetValue(&hadc1);

//        // 4. (可选) 将数字值转换为实际电压值
//        //    假设 Vref = 3.3V, 分辨率 12 位 (4096)
//        voltage = (float)adc_val * 3.3f / 4096.0f; 

//        // (这里可以加入你对 voltage 或 adc_val 的处理逻辑)
//        my_printf(&huart1, "ADC Value: %lu, Voltage: %.2fV\r\n", adc_val, voltage);

//    } 
//    else 
//    {
//        // 转换超时或出错处理
//        // my_printf(&huart1, "ADC Poll Timeout!\n");
//    }
//    
//    // 5. （重要）如果 ADC 配置为单次转换模式，通常不需要手动停止。
//    //    如果是连续转换模式，可能需要 HAL_ADC_Stop(&hadc1);
//    // HAL_ADC_Stop(&hadc1); // 根据你的 CubeMX 配置决定是否需要
//}


/* DMA + 定时处理 */
//记得开启循环模式
// --- 全局变量 --- 
//#define ADC_DMA_BUFFER_SIZE 32 // DMA缓冲区大小，可以根据需要调整
//uint32_t adc_dma_buffer[ADC_DMA_BUFFER_SIZE]; // DMA 目标缓冲区
//__IO uint32_t adc_val;  // 用于存储计算后的平均 ADC 值
//__IO float voltage; // 用于存储计算后的电压值
//extern ADC_HandleTypeDef hadc1;

//// --- 初始化 (通常在 main 函数或外设初始化函数中调用一次) ---
//void adc_dma_init(void)
//{
//    // 启动 ADC 并使能 DMA 传输
//    // hadc1: ADC 句柄
//    // (uint32_t*)adc_dma_buffer: DMA 目标缓冲区地址 (HAL库通常需要uint32_t*)
//    // ADC_DMA_BUFFER_SIZE: 本次DMA传输必须搬运的数据量 (缓冲区大小)
//    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buffer, ADC_DMA_BUFFER_SIZE);
//}

//// --- 处理任务 (在主循环或定时器回调中定期调用) ---
//void adc_task(void)
//{
//    uint32_t adc_sum = 0;
//    
//    // 1. 计算 DMA 缓冲区中所有采样值的总和
//    //    注意：这里直接读取缓冲区，可能包含不同时刻的采样值
//    for(uint16_t i = 0; i < ADC_DMA_BUFFER_SIZE; i++)
//    {
//        adc_sum += adc_dma_buffer[i];
//    }
//    
//    // 2. 计算平均 ADC 值
//    adc_val = adc_sum / ADC_DMA_BUFFER_SIZE; 
//    
//    // 3. (可选) 将平均数字值转换为实际电压值
//    voltage = ((float)adc_val * 3.3f) / 4096.0f; // 假设12位分辨率, 3.3V参考电压

//    // 4. 使用计算出的平均值 (adc_val 或 voltage)
//    my_printf(&huart1, "Average ADC: %lu, Voltage: %.2fV\n", adc_val, voltage);
//}


/* DMA + 定时器 + 块处理 */
// --- 宏定义和外部变量 ---
#define BUFFER_SIZE 1000        // DMA 缓冲区大小 (总点数)

extern DMA_HandleTypeDef hdma_adc1; // 假设这是 ADC1 对应的 DMA 句柄
extern ADC_HandleTypeDef hadc1;    // ADC1 句柄
extern UART_HandleTypeDef huart1; // 用于 my_printf 的 UART 句柄
extern TIM_HandleTypeDef htim3;

// --- 全局变量 ---
uint32_t dac_val_buffer[BUFFER_SIZE / 2]; // 用于存储处理后的 ADC 数据
__IO uint32_t adc_val_buffer[BUFFER_SIZE]; // DMA 目标缓冲区 (存储原始 ADC 数据)

__IO uint8_t AdcConvEnd = 0;             // ADC 转换完成标志 (一个块完成)

// --- 初始化函数 (在 main 或外设初始化后调用) ---
void adc_tim_dma_init(void)
{
    // 启动 ADC 的 DMA 传输，请求 BUFFER_SIZE 个数据点
    // 注意：这里假设 hadc1 已经配置为合适的触发模式 (定时器或软件)
    //       且 DMA 配置为 Normal 模式
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_val_buffer, BUFFER_SIZE);

    // 显式禁用 DMA 半传输中断 (如果不需要处理半满事件)
    __HAL_DMA_DISABLE_IT(&hdma_adc1, DMA_IT_HT);

    // 注意：如果使用定时器触发，需要在此处或之前启动定时器
    HAL_TIM_Base_Start(&htim3); // 替换 htimX 为实际定时器句柄
}

// --- ADC 转换完成回调函数 (由 DMA TC 中断触发) ---
// 当 DMA 完成整个缓冲区的传输 (Normal 模式下传输 BUFFER_SIZE 个点) 时触发
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    // 检查是否是由我们关心的 ADC (hadc1) 触发的
    if (hadc->Instance == ADC1) // 或 if(hadc == &hadc1)
    {
        HAL_ADC_Stop_DMA(hadc);

        // 设置转换完成标志，通知后台任务数据已准备好
        AdcConvEnd = 1;
    }
}

// --- 后台处理任务 (在主循环或低优先级任务中调用) ---
void adc_task(void)
{
    // 检查转换完成标志
    if (AdcConvEnd)
    {
        // 处理数据: 从原始 ADC 缓冲区提取数据到 dac_val_buffer
        // 示例逻辑：提取扫描转换中第二个通道的数据 (?)
        for(uint16_t i = 0; i < BUFFER_SIZE / 2; i++)
        {
					  /*  - 第 1 个转换通道是 /E:/2_Code/GD32/GD32_Demo_05/Core/Src/adc.c:65 的 ADC_CHANNEL_10，Rank = 1
  			        - 第 2 个转换通道是 /E:/2_Code/GD32/GD32_Demo_05/Core/Src/adc.c:75 的 ADC_CHANNEL_5，Rank = 2
								通道2引脚PA5所测的模拟信号是PA4产生的模拟正弦波
            */
            //dac_val_buffer[i] = adc_val_buffer[i * 2];//采集通道1（滑动变阻器）数据
						dac_val_buffer[i] = adc_val_buffer[i * 2 + 1];//采集通道2（DAC输出的模拟sin函数）数据
        }

        // 打印处理后的数据 (示例)
        for(uint16_t i = 0; i < BUFFER_SIZE / 2; i++)
        {
            // 注意: my_printf 是自定义函数, 需确保其存在且可用
					my_printf(&huart1, "<adc>:%d\n", (int)dac_val_buffer[i]);
        }

        // 清理处理后的缓冲区 (可选)
        memset(dac_val_buffer, 0, sizeof(uint32_t) * (BUFFER_SIZE / 2));

        // 清除转换完成标志，准备下一次采集
        AdcConvEnd = 0;

        // 重新启动 ADC 的 DMA 传输，采集下一个数据块
        // 注意: 需要确保 ADC 状态适合重启 (例如没有错误)
        HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_val_buffer, BUFFER_SIZE);
        // 再次禁用半传输中断 (如果 Start_DMA 会重新启用它)
        __HAL_DMA_DISABLE_IT(&hdma_adc1, DMA_IT_HT);
    }
}

// --- 全局变量 --- 
#define SINE_SAMPLES 100    // 一个周期内的采样点数
#define DAC_MAX_VALUE 4095 // 12 位 DAC 的最大数字值 (2^12 - 1)
extern TIM_HandleTypeDef htim6;
extern DAC_HandleTypeDef hdac;
uint16_t SineWave[SINE_SAMPLES]; // 存储正弦波数据的数组

// --- 生成正弦波数据的函数 ---
/**
 * @brief 生成正弦波查找表
 * @param buffer: 存储波形数据的缓冲区指针
 * @param samples: 一个周期内的采样点数
 * @param amplitude: 正弦波的峰值幅度 (相对于中心值)
 * @param phase_shift: 相位偏移 (弧度)
 * @retval None
 */
void Generate_Sine_Wave(uint16_t* buffer, uint32_t samples, uint16_t amplitude, float phase_shift)
{
  // 计算每个采样点之间的角度步进 (2*PI / samples)
  float step = 2.0f * 3.14159f / samples; 
  
  for(uint32_t i = 0; i < samples; i++)
  {
    // 计算当前点的正弦值 (-1.0 到 1.0)
    float sine_value = sinf(i * step + phase_shift); // 使用 sinf 提高效率

    // 将正弦值映射到 DAC 的输出范围 (0 - 4095)
    // 1. 将 (-1.0 ~ 1.0) 映射到 (-amplitude ~ +amplitude)
    // 2. 加上中心值 (DAC_MAX_VALUE / 2)，将范围平移到 (Center-amp ~ Center+amp)
    buffer[i] = (uint16_t)((sine_value * amplitude) + (DAC_MAX_VALUE / 2.0f));
    
    // 确保值在有效范围内 (钳位)
    if (buffer[i] > DAC_MAX_VALUE) buffer[i] = DAC_MAX_VALUE;
    // 由于浮点计算精度问题，理论上不需要检查下限，但加上更健壮
    // else if (buffer[i] < 0) buffer[i] = 0; 
  }
}

// --- 初始化函数 (在 main 函数或外设初始化后调用) ---
void dac_sin_init(void)
{
    // 1. 生成正弦波查找表数据
    //     amplitude = DAC_MAX_VALUE / 2 产生最大幅度的波形 (0-4095)
    Generate_Sine_Wave(SineWave, SINE_SAMPLES, DAC_MAX_VALUE / 2, 0.0f);
    
    // 2. 启动触发 DAC 的定时器 (例如 TIM6)
    HAL_TIM_Base_Start(&htim6); // htim6 是 TIM6 的句柄
    
    // 3. 启动 DAC 通道并通过 DMA 输出查找表数据
    //    hdac: DAC 句柄
    //    DAC_CHANNEL_1: 要使用的 DAC 通道
    //    (uint32_t *)SineWave: 查找表起始地址 (HAL 库常需 uint32_t*)
    //    SINE_SAMPLES: 查找表中的点数 (DMA 传输单元数)
    //    DAC_ALIGN_12B_R: 数据对齐方式 (12 位右对齐)
    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)SineWave, SINE_SAMPLES, DAC_ALIGN_12B_R);
}

// --- 无需后台处理任务 --- 
// 一旦 dac_sin_init 调用完成，硬件会自动循环输出波形
// adc_task() 中可以移除 dac 相关的处理
