/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author           Notes
 * 2019-07-24     WillianChan      the first version
 * 2020-07-28     WillianChan      add the inclusion of the board.h
 * 2024-xx-xx     Gemini           add start/stop commands
 */

#include <stdlib.h>
#include <rtthread.h>
#include <rtdevice.h>
#include "board.h"
#include "dallas_ds18b20_sensor_v1.h"

/* Modify this pin according to the actual wiring situation */
#define DS18B20_DATA_PIN    GET_PIN(E, 11)

/* 添加一个全局标志位来控制线程的运行 */
static rt_bool_t ds18b20_running = RT_FALSE;

static void read_temp_entry(void *parameter)
{
    rt_device_t dev = RT_NULL;
    struct rt_sensor_data sensor_data;
    rt_size_t res;

    dev = rt_device_find(parameter);
    if (dev == RT_NULL)
    {
        rt_kprintf("Can't find device:%s\n", parameter);
        ds18b20_running = RT_FALSE;
        return;
    }

    if (rt_device_open(dev, RT_DEVICE_FLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("open device failed!\n");
        ds18b20_running = RT_FALSE;
        return;
    }
    rt_device_control(dev, RT_SENSOR_CTRL_SET_ODR, (void *)100);

    /* 使用标志位替换原来的 while(1) */
    while (ds18b20_running)
    {
        res = rt_device_read(dev, 0, &sensor_data, 1);
        if (res != 1)
        {
            rt_kprintf("read data failed!size is %d\n", res);
            break; /* 读取失败时退出循环以关闭设备 */
        }
        else
        {
            if (sensor_data.data.temp >= 0)
            {
                rt_kprintf("temp:%3d.%dC, timestamp:%5d\n",
                           sensor_data.data.temp / 10,
                           sensor_data.data.temp % 10,
                           sensor_data.timestamp);
            }
            else
            {
                rt_kprintf("temp:-%2d.%dC, timestamp:%5d\n",
                           abs(sensor_data.data.temp / 10),
                           abs(sensor_data.data.temp % 10),
                           sensor_data.timestamp);
            }
        }
        rt_thread_mdelay(100);
    }

    /* 线程退出前安全地关闭设备 */
    rt_device_close(dev);
    rt_kprintf("ds18b20 thread exit and device closed.\n");
}

/* 导出为 MSH 命令行函数 */
static void ds18b20_cmd(int argc, char *argv[])
{
    if (argc == 2)
    {
        if (!rt_strcmp(argv[1], "start"))
        {
            if (ds18b20_running)
            {
                rt_kprintf("ds18b20 is already running!\n");
                return;
            }

            ds18b20_running = RT_TRUE;
            rt_thread_t ds18b20_thread = rt_thread_create("18b20tem",
                                                          read_temp_entry,
                                                          "temp_ds18b20",
                                                          1024,
                                                          RT_THREAD_PRIORITY_MAX / 2,
                                                          20);
            if (ds18b20_thread != RT_NULL)
            {
                rt_thread_startup(ds18b20_thread);
                rt_kprintf("ds18b20 thread started.\n");
            }
            else
            {
                ds18b20_running = RT_FALSE;
                rt_kprintf("create ds18b20 thread failed!\n");
            }
        }
        else if (!rt_strcmp(argv[1], "stop"))
        {
            if (!ds18b20_running)
            {
                rt_kprintf("ds18b20 is not running!\n");
                return;
            }

            /* 清除标志位，线程在下一次循环检测时会自动退出并销毁 */
            ds18b20_running = RT_FALSE;
            rt_kprintf("ds18b20 stop signal sent.\n");
        }
        else
        {
            rt_kprintf("Usage: ds18b20_cmd start/stop\n");
        }
    }
    else
    {
        rt_kprintf("Usage: ds18b20_cmd start/stop\n");
    }
}
/* 导出命令到 FinSH 终端 */
MSH_CMD_EXPORT(ds18b20_cmd, start/stop the ds18b20 sensor reading);

static int rt_hw_ds18b20_port(void)
{
    struct rt_sensor_config cfg;
    
    cfg.intf.user_data = (void *)DS18B20_DATA_PIN;
    rt_hw_ds18b20_init("ds18b20", &cfg);
    
    return RT_EOK;
}
INIT_COMPONENT_EXPORT(rt_hw_ds18b20_port);
