#include "oled_app.h"

/* oled基本显示

* @brief Use printf-like format to display ASCII string (6x8 font)
* @param x  Start X in pixels (0~127)
* @param y  Start Y in page index (0~3), each page = 8 pixels
* @param format, ... format string and args

*/
int Oled_Printf(uint8_t x, uint8_t y, const char *format, ...)
{
    char buffer[128];
    va_list arg;
    int len;

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if (x > 120)
    {
        x = 0;
    }
    if (y > 3)
    {
        y = 3;
    }

    // OLED_ShowStr expects y as page index, not pixel value.
    OLED_ShowStr(x, y, buffer, 8);
    return len;
}

////oled初始化
void oled_init(void)
{
		OLED_Init();
    OLED_Clear();	
}

//// OLED display task 
//void oled_task(void)
//{
//		static uint8_t count = 250;

//    Oled_Printf(0, 0, "Hello World!!!");
//    Oled_Printf(0, 1, "Welcome to MCU!");
//		Oled_Printf(0, 2, "Count:%d   ",count++);//数据后面应有空格，避免数据由大到小时后面几位没被刷新掉
//		Oled_Printf(0, 3, "End");
//}


/*

		U8G2实现

*/

u8g2_t u8g2; // 全局 u8g2 实例

// u8g2 的 GPIO 和延时回调函数
uint8_t u8g2_gpio_and_delay_stm32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  switch(msg)
  {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
      // 初始化 GPIO (如果需要，例如 SPI 的 CS, DC, RST 引脚)
      // 对于硬件 I2C，这里通常不需要做什么
      break;
    case U8X8_MSG_DELAY_MILLI:
      // 原因: u8g2 内部某些操作需要毫秒级的延时等待。
      // 提供毫秒级延时，直接调用 HAL 库函数。
      HAL_Delay(arg_int);
      break;
    case U8X8_MSG_DELAY_10MICRO:
      // 原因: 某些通信协议或显示时序可能需要微秒级延时。
      // 提供 10 微秒延时。HAL_Delay(1) 精度不够（通常是毫秒级）。
      // 需要更精确的延时，可以使用 CPU NOP 指令或 DWT 定时器。
      // 以下简单循环仅为示例，**需要根据你的 CPU 时钟频率精确调整循环次数**。
      for (volatile uint32_t i = 0; i < 150; ++i) {} // 示例循环，计数需调整
      break;
    case U8X8_MSG_DELAY_100NANO:
      // 原因: 更精密的时序控制，通常在高速接口或特定操作中需要。
      // 提供 100 纳秒延时。非常短，通常用 NOP 指令实现。
      // **同样需要根据 CPU 时钟频率调整 NOP 数量**。
       __NOP(); __NOP(); __NOP(); // 示例 NOP
      break;
    case U8X8_MSG_GPIO_I2C_CLOCK: // [[fallthrough]] // Fallthrough 注释表示有意为之
    case U8X8_MSG_GPIO_I2C_DATA:
      // 控制 SCL/SDA 引脚电平。这些仅在**软件模拟 I2C** 时需要实现。
      // 使用硬件 I2C 时，这些消息可以忽略，由 HAL 库处理。
      break;
     // --- 以下是 GPIO 相关的消息，主要用于按键输入或 SPI 控制 --- 
     // 如果你的 u8g2 应用需要读取按键或控制 SPI 引脚 (CS, DC, Reset)，
     // 你需要在这里根据 msg 类型读取/设置对应的 GPIO 引脚状态。
     // 对于仅使用硬件 I2C 显示的场景，可以像下面这样简单返回不支持。
     case U8X8_MSG_GPIO_CS:
        // SPI 片选控制
        break;
      case U8X8_MSG_GPIO_DC:
        // SPI 数据/命令线控制
        break;
      case U8X8_MSG_GPIO_RESET:
        // 显示屏复位引脚控制
        break;
    case U8X8_MSG_GPIO_MENU_SELECT:
      u8x8_SetGPIOResult(u8x8, /* 读取选择键 GPIO 状态 */ 0);
      break;
    default:
      u8x8_SetGPIOResult(u8x8, 1); // 不支持的消息
      break;
  }
  return 1;
}

// u8g2 的硬件 I2C 通信回调函数
uint8_t u8x8_byte_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  static uint8_t buffer[32]; // u8g2 每次传输最大 32 字节
  static uint8_t buf_idx;
  uint8_t *data;

  switch(msg)
  {
    case U8X8_MSG_BYTE_SEND:
      // 原因: u8g2 通常不会一次性发送大量数据，而是分块发送。
      // 这个消息用于将一小块数据 (arg_int 字节) 从 u8g2 内部传递到我们的回调函数。
      // 我们需要将这些数据暂存到本地 buffer 中，等待 START/END_TRANSFER 信号。
      data = (uint8_t *)arg_ptr;
      while( arg_int > 0 )
      {
        buffer[buf_idx++] = *data;
        data++;
        arg_int--;
      }
      break;
    case U8X8_MSG_BYTE_INIT:
      // 原因: 提供一个机会进行 I2C 外设的初始化。
      // 初始化 I2C (通常在 main 函数中已完成)
      // 由于我们在 main 函数中已经调用了 MX_I2C1_Init()，这里通常可以留空。
      break;
    case U8X8_MSG_BYTE_SET_DC:
      // 原因: 这个消息用于 SPI 通信中控制 Data/Command 选择引脚。
      // 设置数据/命令线 (I2C 不需要)
      // I2C 通过特定的控制字节 (0x00 或 0x40) 区分命令和数据，因此该消息对于 I2C 无意义。
      break;
    case U8X8_MSG_BYTE_START_TRANSFER:
      // 原因: 标记一个 I2C 传输序列的开始。
      buf_idx = 0;
      // 我们在这里重置本地缓冲区的索引，准备接收新的数据块。
      break;
    case U8X8_MSG_BYTE_END_TRANSFER:
      // 原因: 标记一个 I2C 传输序列的结束。
      // 此时，本地 buffer 中已经暂存了所有需要发送的数据块。
      // 这是执行实际 I2C 发送操作的最佳时机。
      // 发送缓冲区中的数据
      // 注意: u8x8_GetI2CAddress(u8x8) 返回的是 7 位地址 * 2 = 8 位地址
      if (HAL_I2C_Master_Transmit(&hi2c1, u8x8_GetI2CAddress(u8x8), buffer, buf_idx, 100) != HAL_OK)
      {
        return 0; // 发送失败
      }
      break;
    default:
      return 0;
  }
  return 1;
}

void oled_u8g2_init(void)
{
  // u8g2 初始化
  // 1. Setup: 这是最关键的一步，它配置了 u8g2 实例。
  //    - 选择与硬件匹配的 setup 函数 (SSD1306, I2C, 128x32, Full Buffer)。
  //    - &u8g2: 指向要配置的 u8g2 结构体实例的指针。
  //    - U8G2_R0: 旋转设置。U8G2_R0=0°, U8G2_R1=90°, U8G2_R2=180°, U8G2_R3=270°。
  //    - u8x8_byte_hw_i2c: 指向你的硬件 I2C 字节传输回调函数的指针。
  //    - u8g2_gpio_and_delay_stm32: 指向你的 GPIO 和延时回调函数的指针。
  u8g2_Setup_ssd1306_i2c_128x32_univision_f(&u8g2, U8G2_R0, u8x8_byte_hw_i2c, u8g2_gpio_and_delay_stm32);
  //    - &u8g2: u8g2 结构体指针
  //    - U8G2_R0: 旋转设置 (0度)
  // 2. Init Display: 发送初始化序列到 OLED
  u8g2_InitDisplay(&u8g2);

  // 3. Set Power Save: 唤醒屏幕。
  //    - 参数 0 表示关闭省电模式 (屏幕亮起)。
  //    - 参数 1 表示进入省电模式 (屏幕熄灭)。
  u8g2_SetPowerSave(&u8g2, 0);

  // (可选) 4. Clear Display: 清空屏幕物理显存。
  //    - 这个函数会立即发送清屏命令到设备。
  //    - 注意与 `u8g2_ClearBuffer` 的区别，后者仅清空内存中的缓冲区。
  // u8g2_ClearDisplay(&u8g2);	
}

///* u8g2 显示任务示例 */
//void oled_task(void) // 或者你定义的任务函数名
//{
//  // --- 准备阶段 ---
//  // 设置绘图颜色 (对于单色屏，1 通常表示点亮像素)
//  u8g2_SetDrawColor(&u8g2, 1);
//  // 选择要使用的字体 (确保字体文件已添加到工程)
//  u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr); // ncenB08: 字体名, _tr: 透明背景

//  // --- 核心绘图流程 ---
//  // 1. 清除内存缓冲区 (非常重要，每次绘制新帧前必须调用)
//  u8g2_ClearBuffer(&u8g2);

//  // 2. 使用 u8g2 API 在缓冲区中绘图
//  //    所有绘图操作都作用于 RAM 中的缓冲区。
//  // 绘制字符串 (参数: u8g2实例, x坐标, y坐标, 字符串)
//  // y 坐标通常是字符串基线的位置。
//  u8g2_DrawStr(&u8g2, 2, 12, "Hello u8g2!"); // 从 (2, 12) 开始绘制
//  u8g2_DrawStr(&u8g2, 2, 28, "Micron Elec Studio"); // 绘制第二行

//  // 绘制图形 (示例：一个空心圆和一个实心框)
//  // 绘制圆 (参数: u8g2实例, 圆心x, 圆心y, 半径, 绘制选项)
//  u8g2_DrawCircle(&u8g2, 90, 19, 10, U8G2_DRAW_ALL); // U8G2_DRAW_ALL 画圆周
//  // 绘制实心框 (参数: u8g2实例, 左上角x, 左上角y, 宽度, 高度)
//  // u8g2_DrawBox(&u8g2, 50, 15, 20, 10);
//  // 绘制空心框 (参数: u8g2实例, 左上角x, 左上角y, 宽度, 高度)
//  // u8g2_DrawFrame(&u8g2, 50, 15, 20, 10);

//  // 3. 将缓冲区内容一次性发送到屏幕 (非常重要)
//  //    这个函数会调用我们之前编写的 I2C 回调函数，将整个缓冲区的数据发送出去。
//  u8g2_SendBuffer(&u8g2);
//}

/**
 * @brief 将 WouoUI 缓冲区的数据发送到 OLED 显示
 * @param buff WouoUI 提供的缓冲区指针，大小为 [高度/8][宽度] 或 [4][128] for 128x32
 */
/**
 * @brief 将 WouoUI 缓冲区的数据发送到 OLED 显示
 * @param buff WouoUI 提供的缓冲区指针，大小为 [高度/8][宽度] 或 [4][128] for 128x32
 */
void OLED_SendBuff(uint8_t buff[4][128])
{
    // 1. 获取 u8g2 内部 RAM 缓冲区的指针
    uint8_t *u8g2_buffer = u8g2_GetBufferPtr(&u8g2);

    // 2. 将 WouoUI 缓冲区的内容完整地复制到 u8g2 的缓冲区中
    //    注意：确保两个缓冲区大小完全一致 (4 * 128 bytes)
    memcpy(u8g2_buffer, buff, 4 * 128);

    // 3. 调用 u8g2 的发送函数，将 u8g2 缓冲区的内容发送到 OLED
    //    这将使用 u8g2 配置的高效 I2C 回调函数来完成传输
    u8g2_SendBuffer(&u8g2);
}
  void oled_page_init(void)
  {
      WouoUI_SelectDefaultUI();
      WouoUI_AttachSendBuffFun(OLED_SendBuff);
      TestUI_Init();
  }
void oled_task(void)
{
	 WouoUI_Proc(10); // 假设任务每 10ms 调用一次，那么oled_task()的调用要修改为1ms调用一次
}

