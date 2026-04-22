#include "oled_app.h"

/**
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

/* OLED display task */
void oled_task(void)
{
    static uint8_t oled_inited = 0;
		static uint8_t count = 0;
    if (!oled_inited)
    {
        OLED_Init();
        OLED_Clear();
        oled_inited = 1;
    }

    Oled_Printf(0, 0, "Hello World!!!");
    Oled_Printf(0, 1, "Welcome to MCU!");
		Oled_Printf(0, 2, "Count:%d",count++);
		Oled_Printf(0, 3, "End");
}
