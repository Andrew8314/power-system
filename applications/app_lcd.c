/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-03-10     Y8314       the first version
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <drv_lcd.h>

// Global variables for sensor data
float global_humidity = 0.0;
float global_temperature = 0.0;
float global_temp_ds18 = 0.0;
float global_voltage = 0.0;
int32_t global_current_ma = 0;
uint8_t global_error_flag = 0;
rt_mutex_t data_mutex;

void update_adc_voltage(void);

void app_lcd_entry(void* parameter) {
    char buffer[64];
    while (1) {
        update_adc_voltage();  // Update ADC
        lcd_clear(WHITE);
        lcd_set_color(WHITE, BLACK);

        // Display AHT10
        rt_snprintf(buffer, sizeof(buffer), "Temp AHT: %d.%d C", (int)global_temperature, (int)(global_temperature * 10) % 10);
        lcd_show_string(10, 10, 16, buffer);
        rt_snprintf(buffer, sizeof(buffer), "Humidity: %d.%d %%", (int)global_humidity, (int)(global_humidity * 10) % 10);
        lcd_show_string(10, 30, 16, buffer);

        // Display DS18B20
        rt_snprintf(buffer, sizeof(buffer), "Temp DS18: %d.%d C", (int)global_temp_ds18, (int)(global_temp_ds18 * 10) % 10);
        lcd_show_string(10, 50, 16, buffer);

        // Display ADC
        rt_snprintf(buffer, sizeof(buffer), "Voltage: %d.%03d V", (int)global_voltage, (int)(global_voltage * 1000) % 1000);
        lcd_show_string(10, 70, 16, buffer);

        // Display CAN
        if (global_error_flag == 0) {
            rt_snprintf(buffer, sizeof(buffer), "Current: %d mA", global_current_ma);
        } else {
            rt_snprintf(buffer, sizeof(buffer), "Current Error: %d", global_error_flag);
        }
        lcd_show_string(10, 90, 16, buffer);

        rt_thread_mdelay(1000);
    }
}

int app_lcd_init(void) {
    data_mutex = rt_mutex_create("data_mutex", RT_IPC_FLAG_PRIO);
    rt_thread_t tid = rt_thread_create("app_lcd", app_lcd_entry, RT_NULL, 2048, 20, 10);
    if (tid != RT_NULL) rt_thread_startup(tid);
    return 0;
}

INIT_DEVICE_EXPORT(app_lcd_init);



