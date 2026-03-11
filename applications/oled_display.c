/*
 * 专属的环境数据 OLED 显示模块
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <stdio.h>
#include "ssd1306.h"
#include <stdlib.h> /* 用于 abs() 函数 */

/* 引入外部全局变量 */
extern float g_aht10_temp;
extern float g_aht10_humi;
extern uint32_t g_adc_voltage_mv;
extern int32_t g_ds18b20_temp;
extern int32_t g_can_current_ma; /* 引入 CAN 电流全局变量 */

static void oled_display_entry(void *parameter)
{
    char buf[32];

    /* 1. 初始化 OLED 屏幕 */
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();

    while (1)
    {
        /* 每次刷新前清屏为黑色 */
        ssd1306_Fill(Black);

        /* 3. 格式化并绘制温度数据 */
        snprintf(buf, sizeof(buf), "Temp: %d.%d C",
                (int)g_aht10_temp,
                (int)(g_aht10_temp * 10) % 10);
        ssd1306_SetCursor(2, 0);
        ssd1306_WriteString(buf, Font_7x10, White);

        /* 4. 格式化并绘制湿度数据 */
        snprintf(buf, sizeof(buf), "Humi: %d.%d %%",
                (int)g_aht10_humi,
                (int)(g_aht10_humi * 10) % 10);
        ssd1306_SetCursor(2, 12);
        ssd1306_WriteString(buf, Font_7x10, White);

        /* 5. 格式化并绘制 ADC 电压数据 */
        snprintf(buf, sizeof(buf), "ADC : %d.%03d V",
                g_adc_voltage_mv / 1000,
                g_adc_voltage_mv % 1000);
        ssd1306_SetCursor(2, 24);
        ssd1306_WriteString(buf, Font_7x10, White);

        /* 4. 格式化并绘制 DS18B20 温度数据 (Y=48) */
        /* 根据 ds18b20_sample.c 的逻辑，传感器原始数据放大了 10 倍 */
        if (g_ds18b20_temp >= 0)
        {
            snprintf(buf, sizeof(buf), "DS18 : %d.%d C",
                    g_ds18b20_temp / 10,
                    g_ds18b20_temp % 10);
        }
        else
        {
            /* 负温处理，利用 abs() 取绝对值 */
            snprintf(buf, sizeof(buf), "DS18 : -%d.%d C",
                    abs(g_ds18b20_temp / 10),
                    abs(g_ds18b20_temp % 10));
        }
        ssd1306_SetCursor(2, 36);
        ssd1306_WriteString(buf, Font_7x10, White);

        /* 5. CAN 总线电流 (Y=48) */
        snprintf(buf, sizeof(buf), "CAN I: %d mA", g_can_current_ma);
        ssd1306_SetCursor(2, 48);
        ssd1306_WriteString(buf, Font_7x10, White);

        /* 6. 更新显存到屏幕 */
        ssd1306_UpdateScreen();

        /* 延时 1000ms 刷新一次 */
        rt_thread_mdelay(1000);
    }
}

int oled_display_init(void)
{
    rt_thread_t tid;

    /* 创建显示线程 */
    tid = rt_thread_create("oled_ui", oled_display_entry, RT_NULL, 1024, 21, 10);

    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
        return RT_EOK;
    }

    return -RT_ERROR;
}
INIT_APP_EXPORT(oled_display_init);
