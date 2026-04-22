#include "usart_app.h"
#include "ringbuffer.h"

/* 串口发送 */
int my_printf(UART_HandleTypeDef *huart, const char *format, ...)
{
	char buffer[512]; // 临时存储格式化后的字符串
	va_list arg;      // 处理可变参数
	int len;          // 最终字符串长度

	va_start(arg, format);
	// 安全地格式化字符串到 buffer
	len = vsnprintf(buffer, sizeof(buffer), format, arg);
	va_end(arg);

	// 通过 HAL 库发送 buffer 中的内容
	HAL_UART_Transmit(huart, (uint8_t *)buffer, (uint16_t)len, 0xFF);
	return len;
}

/* 串口接收 */
/* 1.超时解析法 */
//货架缓冲区
//#define UART_RX_BUFFER_SIZE 128
//uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];

////计数器
//uint16_t uart_rx_index;

////计时器
//uint16_t uart_rx_tick;

////系统时钟
//extern volatile uint32_t uwTick;//同HAL_GetTick()

////超时时间
//#define UART_TIMEOUT_MS 100

////"串口控制器": 这是 HAL 库中代表具体串口硬件（如 USART1）的结构体。我们需要通过它来操作串口，比如启动接收、发送数据等。
//extern UART_HandleTypeDef huart1;

////超时解析串口接收初始化
//void buffer_init(void)
//{
//  	HAL_UART_Receive_IT(&huart1, &uart_rx_buffer[0], 1);//使能串口中断（用于超时解析）
//}

////串口中断回调函数
//void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
//{
//    // 1. 核对身份：是 USART1 的快递员吗？
//    if (huart->Instance == USART1)
//    {
//        // 2. 更新收货时间：记录下当前时间
//        uart_rx_tick = uwTick;
//        // 3. 货物入库：将收到的字节放入缓冲区（HAL库已自动完成）
//        //    并增加计数器
//        //    (注意：实际入库由 HAL_UART_Receive_IT 触发，这里只更新计数)
//        uart_rx_index++;
//        // 4. 准备下次收货：再次告诉硬件，我还想收一个字节
//        HAL_UART_Receive_IT(&huart1, &uart_rx_buffer[uart_rx_index], 1);
//    }
//}

////超时处理任务 uart_task
//void uart_task(void)
//{
//    // 1. 检查货架：如果计数器为0，说明没货或刚处理完，休息。
//	if (uart_rx_index == 0)
//		return;

//    // 2. 检查手表：当前时间 - 最后收货时间 > 规定的超时时间？
//	if (uwTick - uart_rx_tick > UART_TIMEOUT_MS) // 核心判断
//	{
//        // --- 3. 超时！开始理货 --- 
//        // "uart_rx_buffer" 里从第0个到第 "uart_rx_index - 1" 个
//        // 就是我们等到的一整批货（一帧数据）
//		my_printf(&huart1, "uart data: %s\n", uart_rx_buffer);
//        // (在这里加入你自己的处理逻辑，比如解析命令控制LED)
//        // --- 理货结束 --- 

//		// 4. 清理现场：把处理完的货从货架上拿走，计数器归零
//		memset(uart_rx_buffer, 0, uart_rx_index);
//		uart_rx_index = 0;

//    // 5. 将UART接收缓冲区指针重置为接收缓冲区的起始位置
//    huart1.pRxBuffPtr = uart_rx_buffer;
//	}
//    // 如果没超时，啥也不做，等下次再检查
//}


/* 2.DMA+空闲中断法 */
//#define UART_RX_DMA_BUFFER_SIZE 128
//#define UART_DMA_BUFFER_SIZE 128

////"DMA 专用卸货区": 这是 DMA 控制器直接操作的内存区域。
//uint8_t uart_rx_dma_buffer[UART_RX_DMA_BUFFER_SIZE];

////"待处理货架": 当空闲中断发生时，我们会将 DMA 卸货区 (`uart_rx_dma_buffer`) 的数据复制到这个缓冲区进行处理。让DMA能继续接收下一批数据
//uint8_t uart_dma_buffer[UART_DMA_BUFFER_SIZE];

////"到货通知旗": 一个标志位。当空闲中断发生，表示一批数据接收完成并且已经从 DMA 区复制到待处理货架后，我们会把这个旗子举起来 (uart_flag = 1;)。
//volatile uint8_t uart_flag = 0;

////DMA初始化
//void buffer_init(void)
//{
//		HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer, sizeof(uart_rx_dma_buffer));//使能空闲中断（用于DMA+空闲中断）
//  	__HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);	
//}

///**
// * @brief UART DMA接收完成或空闲事件回调函数
// * @param huart UART句柄
// * @param Size 指示在事件发生前，DMA已经成功接收了多少字节的数据
// * @retval None
// */
////空闲中断回调函数 HAL_UARTEx_RxEventCallback()
//void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
//{
//    // 1. 确认是目标串口 (USART1)
//    if (huart->Instance == USART1)
//    {
//        // 2. 紧急停止当前的 DMA 传输 (如果还在进行中)
//        //    因为空闲中断意味着发送方已经停止，防止 DMA 继续等待或出错
//        HAL_UART_DMAStop(huart);

//        // 3. 将 DMA 缓冲区中有效的数据 (Size 个字节) 复制到待处理缓冲区
//        memcpy(uart_dma_buffer, uart_rx_dma_buffer, Size); 
//        // 注意：这里使用了 Size，只复制实际接收到的数据
//        
//        // 4. 举起"到货通知旗"，告诉主循环有数据待处理
//        uart_flag = 1;

//        // 5. 清空 DMA 接收缓冲区，为下次接收做准备
//        //    虽然 memcpy 只复制了 Size 个，但清空整个缓冲区更保险
//        memset(uart_rx_dma_buffer, 0, sizeof(uart_rx_dma_buffer));

//        // 6. **关键：重新启动下一次 DMA 空闲接收**
//        // 必须再次调用，否则只会接收这一次
//        // 由于DMA配置的模式是Normal？
//        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer, sizeof(uart_rx_dma_buffer));
//        
//        // 7. 如果之前关闭了半满中断，可能需要在这里再次关闭 (根据需要)
//        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
//    }
//}

////处理任务 uart_task()
///**
// * @brief  处理 DMA 接收到的 UART 数据
// * @param  None
// * @retval None
// */
//void uart_task(void)
//{
//    // 1. 检查"到货通知旗"
//    if(uart_flag == 0) 
//        return; // 旗子没举起来，说明没新货，直接返回
//    
//    // 2. 放下旗子，表示我们已经注意到新货了
//    //    防止重复处理同一批数据
//    uart_flag = 0;
//	
//    // 3. 处理 "待处理货架" (uart_dma_buffer) 中的数据
//    //    这里简单地打印出来，实际应用中会进行解析、执行命令等
//    my_printf(&huart1,"DMA data: %s\n", uart_dma_buffer);
//    //    (注意：如果数据不是字符串，需要用其他方式处理，比如按字节解析)
//    
//    // 4. 清空"待处理货架"，为下次接收做准备
//    memset(uart_dma_buffer, 0, sizeof(uart_dma_buffer));
//}


/* 3.DMA+空闲中断+环形缓冲区 */
// DMA 缓冲区大小（必须是固定大小，例如 256 或 512）
#define UART1_DMA_BUF_SIZE 128

// 环形缓冲区 (Ring Buffer) 大小，应大于 DMA 缓冲区，以提供缓冲余量
// 我们使用 512 字节作为例子
#define UART1_RING_BUF_SIZE 512 

// DMA 专用卸货区
uint8_t uart_rx_dma_buffer[UART1_DMA_BUF_SIZE];

// 环形缓冲区存储空间
static rt_uint8_t uart1_rx_ringbuffer_pool[UART1_RING_BUF_SIZE];

// 环形缓冲区控制结构体
static struct rt_ringbuffer uart1_rx_rb;

//环形缓冲区初始化
void ringbuffer_init(void)
{
    // 初始化环形缓冲区
    rt_ringbuffer_init(&uart1_rx_rb,uart1_rx_ringbuffer_pool,UART1_RING_BUF_SIZE);

    // 启动 DMA 接收
    // 首次启动，使用 DMA 缓冲区接收
	
//  	HAL_UART_Receive_IT(&huart1, &uart_rx_buffer[0], 1);//使能串口中断（用于超时解析）
	
//		HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer, sizeof(uart_rx_dma_buffer));//使能空闲中断（用于DMA+空闲中断）
//  	__HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);	


		HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer, UART1_DMA_BUF_SIZE);
		__HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
    // 如果 DMA 是循环模式 (Circular Mode)，则不需要在 IDLE 中重启。
    // 但是 HAL_UARTEx_ReceiveToIdle_DMA 内部通常使用 Normal 模式，所以需要在回调中重新启动。
}

/**
 * @brief UART DMA接收完成或空闲事件回调函数 (ISR 上下文)
 * @param huart UART句柄
 * @param Size 指示在事件发生前，DMA已经成功接收了多少字节的数据
 * @retval None
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    uint16_t data_len;

    if (huart->Instance == USART1)
    {
        // 1. 停止当前 DMA 传输
        HAL_UART_DMAStop(huart);
        
        // Size 是本次接收到的数据长度
        data_len = Size;

        // 2. 将 DMA 缓冲区中的数据拷贝到环形缓冲区
        // 核心改动：用 rt_ringbuffer_put 替换 memcpy
        rt_ringbuffer_put(&uart1_rx_rb,uart_rx_dma_buffer,data_len); 

        // 3. 清空 DMA 接收缓冲区 (虽然不是必须，但对于调试和防止残留数据是好的习惯)
        memset(uart_rx_dma_buffer, 0, data_len); 
        
        // 4. 重新启动下一次 DMA 空闲接收
        // 必须再次调用，以准备好接收下一批数据
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma_buffer, UART1_DMA_BUF_SIZE);
        
        // 5. DMA_IT_HT (半满中断) 处理 (如果需要，通常在 IDLE 模式下不使用)
        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
    }
}

#define PROCESS_BUFFER_SIZE 128
uint8_t process_buffer[PROCESS_BUFFER_SIZE]; // 用于临时存储从环形缓冲区取出的数据

/**
 * @brief  处理 DMA 接收到的 UART 数据 (主循环上下文)
 * @param  None
 * @retval None
 */
void uart_task(void)
{
    rt_size_t read_len;
    
    // 1. 检查环形缓冲区中是否有数据
    read_len = rt_ringbuffer_data_len(&uart1_rx_rb);
    
    if (read_len == 0)
    {
        return; // 缓冲区空，返回
    }

    // 2. 读取数据：我们一次性读取不超过 PROCESS_BUFFER_SIZE 的数据
    // 也可以选择使用 rt_ringbuffer_getchar 逐字节处理，取决于您的协议。
    if (read_len > PROCESS_BUFFER_SIZE)
    {
        read_len = PROCESS_BUFFER_SIZE;
    }
    
    // 从环形缓冲区中取出数据
    rt_ringbuffer_get(&uart1_rx_rb, process_buffer, read_len);

    // 3. 处理数据
    // 如果您处理的是字符串，需要确保在取出的 read_len 范围内添加字符串终止符
    if (read_len > 0)
    {
        // 确保process_buffer以\0结尾，便于my_printf按字符串打印
        if (read_len < PROCESS_BUFFER_SIZE)
        {
            process_buffer[read_len] = '\0';
        }
        else
        {
            // 如果刚好满了，最后一位可能不是\0，仅供参考，实际应用中应该按字节解析
            // 或者使用 my_printf_buffer(process_buffer, read_len)
        }
        
        my_printf(&huart1, "Received data from ringbuffer: %s (Length: %d)\n", process_buffer, read_len);
    }
    //注意：不需要手动清空 process_buffer，因为 rt_ringbuffer_get 已经移动了读指针。

}


