#include "usart_app.h"
#include "ringbuffer.h"

extern DMA_HandleTypeDef hdma_usart1_rx;
extern UART_HandleTypeDef huart1;

/*
 * 串口接收策略：
 * 1. USART1 使用 DMA 接收到空闲中断。
 * 2. DMA 回调里把数据搬进环形缓冲区。
 * 3. 主循环中的 uart_task() 再从环形缓冲区读出数据。
 * 4. 通过“按行拼接”的方式解析命令。
 *
 * 这样做的优点是：
 * - 中断里不做复杂字符串处理，响应更轻量。
 * - 主循环中可以方便地做命令解析和参数设置。
 */

/* USART1 的 DMA 接收缓冲区长度 */
#define UART1_DMA_BUF_SIZE      128U
/* 环形缓冲区总长度，应大于 DMA 缓冲区，给串口突发数据留余量 */
#define UART1_RING_BUF_SIZE     512U
/* 主循环单次最多处理多少字节串口数据 */
#define PROCESS_BUFFER_SIZE     128U
/* 单条命令行允许的最大长度 */
#define UART_CMD_LINE_SIZE      128U

/* DMA 直接写入的接收缓冲区 */
static uint8_t uart_rx_dma_buffer[UART1_DMA_BUF_SIZE];
/* 环形缓冲区底层存储池 */
static rt_uint8_t uart1_rx_ringbuffer_pool[UART1_RING_BUF_SIZE];
/* 环形缓冲区控制块 */
static struct rt_ringbuffer uart1_rx_rb;
/* 主循环中处理命令时使用的临时缓冲区 */
static uint8_t process_buffer[PROCESS_BUFFER_SIZE];
/* 当前正在拼接的一行命令 */
static char uart_cmd_line[UART_CMD_LINE_SIZE];
/* 当前命令行已写入的字符数 */
static uint16_t uart_cmd_index = 0U;

/**
 * @brief 打印串口帮助信息
 *
 * 说明：
 * 这部分命令集就是当前工程支持的串口协议。
 * 后续如果要扩展查询项或设置项，优先同步更新这里。
 */
static void uart_print_help(void)
{
    my_printf(&huart1, "help\r\n");
    my_printf(&huart1, "get all|get wave|get freq|get vpp\r\n");
    my_printf(&huart1, "set wave sine|square|triangle\r\n");
    my_printf(&huart1, "set freq <hz>\r\n");
    my_printf(&huart1, "set period <ms>\r\n");
    my_printf(&huart1, "set vpp <volt>\r\n");
    my_printf(&huart1, "print start|stop\r\n");
}

/**
 * @brief 把一批串口字节流送入命令拼接器
 * @param data   接收到的字节流
 * @param length 字节数
 *
 * 处理规则：
 * 1. 遇到 '\r' 或 '\n'，认为一条命令结束。
 * 2. 如果命令长度超过上限，则丢弃本条命令并提示错误。
 * 3. 命令按纯文本处理，适合串口助手手工输入。
 */
static void uart_feed_bytes(const uint8_t *data, uint16_t length)
{
    uint16_t i;

    for (i = 0U; i < length; i++)
    {
        char ch = (char)data[i];

        if ((ch == '\r') || (ch == '\n'))
        {
            if (uart_cmd_index > 0U)
            {
                uart_cmd_line[uart_cmd_index] = '\0';
                uart_cmd_parse(uart_cmd_line);
                uart_cmd_index = 0U;
                memset(uart_cmd_line, 0, sizeof(uart_cmd_line));
            }
        }
        else if (uart_cmd_index < (UART_CMD_LINE_SIZE - 1U))
        {
            uart_cmd_line[uart_cmd_index++] = ch;
        }
        else
        {
            uart_cmd_index = 0U;
            memset(uart_cmd_line, 0, sizeof(uart_cmd_line));
            my_printf(&huart1, "cmd too long\r\n");
        }
    }
}

int my_printf(UART_HandleTypeDef *huart, const char *format, ...)
{
    char buffer[512];
    va_list arg;
    int len;

    /*
     * 这里封装一个最简单的串口 printf。
     * 先用 vsnprintf 格式化到本地缓冲区，再调用 HAL 阻塞发送。
     *
     * 注意：
     * 1. 这是阻塞发送，打印大量波形数据时会影响实时性。
     * 2. 当前项目用于联调和作业演示是够用的。
     */
    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (len < 0)
    {
        return len;
    }

    if (len >= (int)sizeof(buffer))
    {
        len = (int)sizeof(buffer) - 1;
    }

    HAL_UART_Transmit(huart, (uint8_t *)buffer, (uint16_t)len, 0xFF);
    return len;
}

void ringbuffer_init(void)
{
    /*
     * 初始化环形缓冲区，并启动 USART1 的 DMA + 空闲中断接收。
     * 关闭半传输中断，是因为当前只关心“收到一段完整数据”这一事件。
     */
    rt_ringbuffer_init(&uart1_rx_rb, uart1_rx_ringbuffer_pool, UART1_RING_BUF_SIZE);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer, UART1_DMA_BUF_SIZE);
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    /*
     * 这个回调会在 DMA 接收完成或串口空闲时触发。
     * 我们这里只做两件事：
     * 1. 把这批数据塞进环形缓冲区
     * 2. 立刻重启下一轮 DMA 接收
     *
     * 注意：不要在这里解析命令，避免 ISR 过重。
     */
    if (huart->Instance == USART1)
    {
        HAL_UART_DMAStop(huart);
        rt_ringbuffer_put(&uart1_rx_rb, uart_rx_dma_buffer, Size);
        memset(uart_rx_dma_buffer, 0, Size);
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer, UART1_DMA_BUF_SIZE);
        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
    }
}

void uart_print_status(void)
{
    const wave_ctrl_t *ctrl = dac_get_ctrl();

    if (ctrl == NULL)
    {
        return;
    }

    /*
     * 这里统一打印两组信息：
     * 1. [set] : 当前软件设定值
     * 2. [adc] : 由 ADC 回采分析得到的实际测量值
     *
     * 这两组数据并存，便于答辩时解释：
     * “设定值”和“测量值”可能存在少量误差，但整体趋势一致。
     */
    my_printf(&huart1,
              "[set] wave=%s freq=%.2fHz period=%.2fms vpp=%.2fV print=%u\r\n",
              wave_type_to_string(ctrl->wave_type),
              ctrl->freq_hz,
              ctrl->period_ms,
              ctrl->vpp_set,
              ctrl->print_enable);

    my_printf(&huart1,
              "[adc] wave=%s freq=%.2fHz period=%.2fms vpp=%.2fV period_samples=%u\r\n",
              wave_type_to_string(g_wave_meas.calc_type),
              g_wave_meas.calc_freq_hz,
              g_wave_meas.calc_period_ms,
              g_wave_meas.calc_vpp,
              g_wave_meas.period_samples);
}

void uart_cmd_parse(char *cmd_line)
{
    char wave_name[16];
    float value;

    if (cmd_line == NULL)
    {
        return;
    }

    /*
     * 当前串口协议分三类：
     * 1. 帮助命令：help / ?
     * 2. 查询命令：get ...
     * 3. 设置命令：set ...
     *
     * 注意：
     * 每次设置完波形、频率、周期、峰峰值后，
     * 都会主动等待一块新的 ADC 数据并刷新测量结果，
     * 这样串口返回时看到的 [adc] 就是“新参数下”的结果，
     * 不会再出现“上一帧旧值”的延迟现象。
     */
    if ((strcmp(cmd_line, "help") == 0) || (strcmp(cmd_line, "?") == 0))
    {
        uart_print_help();
    }
    else if (strcmp(cmd_line, "get all") == 0)
    {
        uart_print_status();
    }
    else if (strcmp(cmd_line, "get wave") == 0)
    {
        my_printf(&huart1, "[adc] wave=%s\r\n", wave_type_to_string(g_wave_meas.calc_type));
    }
    else if (strcmp(cmd_line, "get freq") == 0)
    {
        my_printf(&huart1,
                  "[adc] freq=%.2fHz period=%.2fms\r\n",
                  g_wave_meas.calc_freq_hz,
                  g_wave_meas.calc_period_ms);
    }
    else if (strcmp(cmd_line, "get vpp") == 0)
    {
        my_printf(&huart1, "[adc] vpp=%.2fV\r\n", g_wave_meas.calc_vpp);
    }
    else if (sscanf(cmd_line, "set wave %15s", wave_name) == 1)
    {
        if (strcmp(wave_name, "sine") == 0)
        {
            wave_set_type(WAVE_TYPE_SINE);
        }
        else if (strcmp(wave_name, "square") == 0)
        {
            wave_set_type(WAVE_TYPE_SQUARE);
        }
        else if (strcmp(wave_name, "triangle") == 0)
        {
            wave_set_type(WAVE_TYPE_TRIANGLE);
        }
        else
        {
            my_printf(&huart1, "unknown wave type\r\n");
            return;
        }

        wave_refresh_measurement_now(150U);
        uart_print_status();
    }
    else if (sscanf(cmd_line, "set freq %f", &value) == 1)
    {
        wave_set_frequency(value);
        wave_refresh_measurement_now(150U);
        uart_print_status();
    }
    else if (sscanf(cmd_line, "set period %f", &value) == 1)
    {
        wave_set_period(value);
        wave_refresh_measurement_now(150U);
        uart_print_status();
    }
    else if (sscanf(cmd_line, "set vpp %f", &value) == 1)
    {
        wave_set_vpp(value);
        wave_refresh_measurement_now(150U);
        uart_print_status();
    }
    else if (strcmp(cmd_line, "print start") == 0)
    {
        dac_set_print_enable(1U);
        uart_print_status();
    }
    else if (strcmp(cmd_line, "print stop") == 0)
    {
        dac_set_print_enable(0U);
        uart_print_status();
    }
    else
    {
        my_printf(&huart1, "unknown cmd: %s\r\n", cmd_line);
    }
}

void uart_task(void)
{
    rt_size_t read_len;

    /*
     * 主循环里定期检查环形缓冲区是否有新数据。
     * 有的话就分批取出，再交给 uart_feed_bytes() 做命令拼接。
     */
    read_len = rt_ringbuffer_data_len(&uart1_rx_rb);
    if (read_len == 0U)
    {
        return;
    }

    if (read_len > PROCESS_BUFFER_SIZE)
    {
        read_len = PROCESS_BUFFER_SIZE;
    }

    memset(process_buffer, 0, sizeof(process_buffer));
    rt_ringbuffer_get(&uart1_rx_rb, process_buffer, (rt_uint16_t)read_len);
    uart_feed_bytes(process_buffer, (uint16_t)read_len);
}
