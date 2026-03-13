/*
 * 专属的环境数据 OLED 显示模块
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <stdio.h>
#include "ssd1306.h"
#include <stdlib.h> /* 用于 abs() 函数 */
#include <board.h>
#include <time.h> /* 新增：引入时间库以获取 RTC 时间 */

/* 替换为您实际连接按键的引脚 */
#define PIN_KEY_TOGGLE  GET_PIN(C, 1)

/* 引入外部全局变量 */
//extern float g_aht10_temp;
//extern float g_aht10_humi;
extern uint32_t g_adc_voltage_mv;
extern int32_t g_ds18b20_temp;
extern int32_t g_can_current_ma; /* 引入 CAN 电流全局变量 */
extern uint32_t g_adc_voltage_mv_ch1;
extern uint8_t g_sys_alarm_status;

static void oled_display_entry(void *parameter)
{
    char buf[32];
    /* 解决 Error: 必须在函数开头声明时间变量 */
    time_t now;
    struct tm *tm_info;
    /* 1. 初始化 OLED 屏幕 */
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();

    while (1)
    {
        /* 每次刷新前清屏为黑色 */
        ssd1306_Fill(Black);

        /* 获取系统当前时间 */
        now = time(RT_NULL);
        tm_info = localtime(&now);

        /* 1. 格式化并绘制日期 (Y=0) */
        snprintf(buf, sizeof(buf), "Date: %04d-%02d-%02d",
                tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday);
        ssd1306_SetCursor(2, 0);
        ssd1306_WriteString(buf, Font_7x10, White);

        /* 2. 格式化并绘制时间 (Y=12) */
        snprintf(buf, sizeof(buf), "Time: %02d:%02d:%02d",
                tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
        ssd1306_SetCursor(2, 12);
        ssd1306_WriteString(buf, Font_7x10, White);
        //        /* 3. 格式化并绘制温度数据 */
        //        snprintf(buf, sizeof(buf), "Temp: %d.%d C",
        //                (int)g_aht10_temp,
        //                (int)(g_aht10_temp * 10) % 10);
        //        ssd1306_SetCursor(2, 0);
        //        ssd1306_WriteString(buf, Font_7x10, White);
        //
        //        /* 4. 格式化并绘制湿度数据 */
        //        snprintf(buf, sizeof(buf), "Humi: %d.%d %%",
        //                (int)g_aht10_humi,
        //                (int)(g_aht10_humi * 10) % 10);
        //        ssd1306_SetCursor(2, 12);
        //        ssd1306_WriteString(buf, Font_7x10, White);

        /* 5. 格式化并绘制 ADC 电压数据 (左右并排显示 A0 和 A1) */
        snprintf(buf, sizeof(buf), "A0:%d.%02d A1:%d.%02d",
                (int)(g_adc_voltage_mv / 1000), (int)((g_adc_voltage_mv % 1000) / 10),
                (int)(g_adc_voltage_mv_ch1 / 1000), (int)((g_adc_voltage_mv_ch1 % 1000) / 10));
        ssd1306_SetCursor(2, 24);
        ssd1306_WriteString(buf, Font_7x10, White);

        /* 4. 格式化并绘制 DS18B20 温度数据 (Y=48) */
        /* 根据 ds18b20_sample.c 的逻辑，传感器原始数据放大了 10 倍 */
        if (g_ds18b20_temp >= 0)
        {
            snprintf(buf, sizeof(buf), "DS18: %d.%d C",
                    (int)(g_ds18b20_temp / 10), (int)(g_ds18b20_temp % 10));
        }
        else
        {
            snprintf(buf, sizeof(buf), "DS18: -%d.%d C",
                    (int)abs(g_ds18b20_temp / 10), (int)abs(g_ds18b20_temp % 10));
        }
        ssd1306_SetCursor(2, 36);
        ssd1306_WriteString(buf, Font_7x10, White);

        /* 5. CAN 总线电流 (Y=48) */
        snprintf(buf, sizeof(buf), "CAN I: %d mA", (int)g_can_current_ma);
        ssd1306_SetCursor(2, 48);
        ssd1306_WriteString(buf, Font_7x10, White);

        /* 6. 异常状态屏幕闪烁提示 */
                if (g_sys_alarm_status != 0)
                {
                    /* 利用系统时间秒数的奇偶性，实现 1秒亮，1秒灭的闪烁效果 */
                    if (tm_info->tm_sec % 2 == 0)
                    {
                        ssd1306_SetCursor(85, 48); /* 放在屏幕底行靠右的位置 */
                        ssd1306_WriteString("WARN!", Font_7x10, White);
                    }
                }

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


/* 按键轮询线程入口函数 */
static void key_thread_entry(void *parameter)
{
    /* 1. 设置按键引脚为上拉输入模式 */
    rt_pin_mode(PIN_KEY_TOGGLE, PIN_MODE_INPUT_PULLUP);

    uint8_t key_pressed = 0; /* 按键按下状态标志位 */

    while (1)
    {
        /* 2. 检测按键是否被按下（低电平有效） */
        if (rt_pin_read(PIN_KEY_TOGGLE) == PIN_LOW)
        {
            rt_thread_mdelay(20); /* 软件防抖延时 20ms */

            /* 3. 再次确认按键按下，并且之前未被标记为按下状态 */
            if (rt_pin_read(PIN_KEY_TOGGLE) == PIN_LOW && key_pressed == 0)
            {
                key_pressed = 1; /* 标记按键已按下，防止长按时疯狂触发 */

                /* 4. 获取当前屏幕状态并进行翻转 */
                if (ssd1306_GetDisplayOn() == 1)
                {
                    ssd1306_SetDisplayOn(0); /* 如果当前是开的，则关屏 */
                    rt_kprintf("OLED Display OFF\n");
                }
                else
                {
                    ssd1306_SetDisplayOn(1); /* 如果当前是关的，则开屏 */
                    rt_kprintf("OLED Display ON\n");
                }
            }
        }
        else
        {
            /* 按键已释放，清除按下标志位 */
            key_pressed = 0;
        }

        /* 5. 线程休眠，让出 CPU 给其他传感器读取线程 */
        rt_thread_mdelay(50);
    }
}

/* 自动初始化并启动按键线程 */
int key_control_init(void)
{
    rt_thread_t tid = rt_thread_create("key_ctrl",
            key_thread_entry,
            RT_NULL,
            1024,
            24,   /* 优先级可设置在 OLED 显示线程之下 */
            10);
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
        return RT_EOK;
    }
    return -RT_ERROR;
}
/* 导出到应用层自动初始化阶段，开机自动运行 */
INIT_APP_EXPORT(key_control_init);

