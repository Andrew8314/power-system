/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author           Notes
 * 2019-07-24     WillianChan      the first version
 * 2020-07-28     WillianChan      add the inclusion of the board.h
 * 2026-xx-xx     User             modify to auto-start thread
 */

#include <stdlib.h>
#include <rtthread.h>
#include <rtdevice.h>
#include "board.h"
#include "dallas_ds18b20_sensor_v1.h"

/* Modify this pin according to the actual wiring situation */
#define DS18B20_DATA_PIN    GET_PIN(E, 11)

/* 新增全局变量，供 OLED 显示线程读取 */
int32_t g_ds18b20_temp = 0;

static void read_temp_entry(void *parameter)
{
    rt_device_t dev = RT_NULL;
    struct rt_sensor_data sensor_data;
    rt_size_t res;

    /* 延时等待底层传感器设备注册完成 */
    rt_thread_mdelay(1000);

    dev = rt_device_find(parameter);
    if (dev == RT_NULL)
    {
        rt_kprintf("Can't find device:%s\n", parameter);
        return;
    }

    if (rt_device_open(dev, RT_DEVICE_FLAG_RDWR) != RT_EOK)
    {
        rt_kprintf("open device failed!\n");
        return;
    }
    rt_device_control(dev, RT_SENSOR_CTRL_SET_ODR, (void *)100);

    /* 改为死循环，实现自动持续触发读取 */
    while (1)
    {
        res = rt_device_read(dev, 0, &sensor_data, 1);
        if (res != 1)
        {
            rt_kprintf("read data failed!size is %d\n", res);
            rt_thread_mdelay(1000); /* 读取失败时增加延时，防止死循环刷屏 */
            continue;
        }

        /* 将温度值更新到全局变量供 OLED 刷新 */
        g_ds18b20_temp = sensor_data.data.temp;

        /* 保留串口打印，方便调试核对数据 */
//        if (sensor_data.data.temp >= 0)
//        {
//            rt_kprintf("[DS18B20] temp:%3d.%dC, timestamp:%5d\n",
//                       sensor_data.data.temp / 10,
//                       sensor_data.data.temp % 10,
//                       sensor_data.timestamp);
//        }
//        else
//        {
//            rt_kprintf("[DS18B20] temp:-%2d.%dC, timestamp:%5d\n",
//                       abs(sensor_data.data.temp / 10),
//                       abs(sensor_data.data.temp % 10),
//                       sensor_data.timestamp);
//        }

        /* 调整读取频率为 1000ms，与 OLED 的刷新频率对齐以节省 CPU 资源 */
        rt_thread_mdelay(1000);
    }
}

/* 自动启动线程的应用层初始化函数 */
int ds18b20_app_init(void)
{
    rt_thread_t ds18b20_thread = rt_thread_create("18b20tem",
                                                  read_temp_entry,
                                                  "temp_ds18b20",
                                                  1024,
                                                  RT_THREAD_PRIORITY_MAX / 2,
                                                  20);
    if (ds18b20_thread != RT_NULL)
    {
        rt_thread_startup(ds18b20_thread);
        return RT_EOK;
    }

    rt_kprintf("create ds18b20 thread failed!\n");
    return -RT_ERROR;
}
/* 导出到自动初始化环节，系统开机后在应用层阶段自动拉起 */
INIT_APP_EXPORT(ds18b20_app_init);

/* 底层硬件端口初始化保持不变 */
static int rt_hw_ds18b20_port(void)
{
    struct rt_sensor_config cfg;
    
    cfg.intf.user_data = (void *)DS18B20_DATA_PIN;
    rt_hw_ds18b20_init("ds18b20", &cfg);
    
    return RT_EOK;
}
/* 在组件阶段完成硬件接口的注册，确保先于应用层初始化 */
INIT_COMPONENT_EXPORT(rt_hw_ds18b20_port);
