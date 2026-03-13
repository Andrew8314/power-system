/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-03-13     Y8314       the first version
 */
/*
 * File      : alarm_monitor.c
 * 说明      : 异常数据监控与报警处理线程
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>

#define DBG_TAG "app.alarm"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* 引入现有的外部全局变量 */
extern int32_t g_ds18b20_temp;
extern uint32_t g_adc_voltage_mv;
extern uint32_t g_adc_voltage_mv_ch1;
extern int32_t g_can_current_ma;

/* 新增：全局报警状态标志 (0为正常，非0为异常)
 * Bit 0: 温度异常
 * Bit 1: ADC V0 电压异常
 * Bit 2: ADC V1 电压异常
 * Bit 3: CAN 电流异常
 */
uint8_t g_sys_alarm_status = 0;

/* 报警输出引脚：这里借用 main.c 中的红色指示灯 PF12，如果有蜂鸣器可改为蜂鸣器引脚 */
#define PIN_ALARM_OUT  GET_PIN(F, 11)

/* ---------------- 异常阈值设定 (可根据实际情况修改) ---------------- */
#define THRESHOLD_TEMP_MAX     240   /* DS18B20 温度 > 24.0 °C */
#define THRESHOLD_VOLTAGE_MAX  3000  /* ADC 电压 > 3.0 V (3000mV) */
#define THRESHOLD_CURRENT_MAX  10  /* CAN 电流 > 10 mA */

static void alarm_monitor_entry(void *parameter)
{
    /* 设置报警引脚为输出模式，并初始化为高电平（默认灭） */
    rt_pin_mode(PIN_ALARM_OUT, PIN_MODE_OUTPUT);
    rt_pin_write(PIN_ALARM_OUT, PIN_HIGH);

    while (1)
    {
        uint8_t current_alarm = 0;

        /* 1. 巡检温度 */
        if (g_ds18b20_temp > THRESHOLD_TEMP_MAX) {
            current_alarm |= 0x01;
        }
        /* 2. 巡检电压 */
        if (g_adc_voltage_mv > THRESHOLD_VOLTAGE_MAX) {
            current_alarm |= 0x02;
        }
        if (g_adc_voltage_mv_ch1 > THRESHOLD_VOLTAGE_MAX) {
            current_alarm |= 0x04;
        }
        /* 3. 巡检电流 */
        if (g_can_current_ma > THRESHOLD_CURRENT_MAX) {
            current_alarm |= 0x08;
        }

        /* 更新全局状态供其他模块读取 */
        g_sys_alarm_status = current_alarm;

        /* 触发本地声光报警动作 */
        if (g_sys_alarm_status != 0)
        {
            /* 发生异常：引脚拉低，点亮红灯 / 响蜂鸣器 */
            rt_pin_write(PIN_ALARM_OUT, PIN_LOW);
            LOG_W("ALARM TRIGGERED! Code: 0x%02X", g_sys_alarm_status);
        }
        else
        {
            /* 恢复正常：关闭报警 */
            rt_pin_write(PIN_ALARM_OUT, PIN_HIGH);
        }

        /* 每 500ms 巡检一次 */
        rt_thread_mdelay(500);
    }
}

int alarm_monitor_init(void)
{
    rt_thread_t tid = rt_thread_create("alarm_mon", alarm_monitor_entry, RT_NULL, 1024, 20, 10);
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
        return RT_EOK;
    }
    return -RT_ERROR;
}
INIT_APP_EXPORT(alarm_monitor_init);


