/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-03-11     Y8314       the first version
 */
/*
 * 专属的环境数据 OLED 显示模块
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <stdio.h>
#include "ssd1306.h"

/* 引入 a_ath10.c 中定义的全局变量 */
extern float g_aht10_temp;
extern float g_aht10_humi;

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

        /* 2. 绘制静态标题 (使用 11x18 字体) */
        ssd1306_SetCursor(2, 0);
        ssd1306_WriteString("Env Monitor", Font_11x18, White);

        /* 3. 格式化并绘制温度数据 */
        /* 采用避开 %f 浮点打印的技巧，与传感器原始日志格式保持一致 */
        snprintf(buf, sizeof(buf), "Temp: %d.%d C",
                 (int)g_aht10_temp,
                 (int)(g_aht10_temp * 10) % 10);
        ssd1306_SetCursor(2, 24);
        ssd1306_WriteString(buf, Font_11x18, White);

        /* 4. 格式化并绘制湿度数据 */
        snprintf(buf, sizeof(buf), "Humi: %d.%d %%",
                 (int)g_aht10_humi,
                 (int)(g_aht10_humi * 10) % 10);
        ssd1306_SetCursor(2, 44);
        ssd1306_WriteString(buf, Font_11x18, White);

        /* 5. 更新显存到屏幕 */
        ssd1306_UpdateScreen();

        /* 延时 1000ms 刷新一次，与 AHT10 采集频率匹配 */
        rt_thread_mdelay(1000);
    }
}

int oled_display_init(void)
{
    rt_thread_t tid;

    /* 创建显示线程，优先级设为 21 (略低于传感器的 20，确保先采后显) */
    tid = rt_thread_create("oled_ui", oled_display_entry, RT_NULL, 1024, 21, 10);

    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
        return RT_EOK;
    }

    return -RT_ERROR;
}
/* 导出到自动初始化环节，系统启动后自动运行 */
INIT_APP_EXPORT(oled_display_init);


