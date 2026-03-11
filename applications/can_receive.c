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
#include "rtdevice.h"

#define CAN_DEV_NAME       "can1"      /* CAN 设备名称 */
#define TARGET_CAN_ID      0x3C2       /* 目标传感器 ID */

static struct rt_semaphore rx_sem;     /* 用于接收消息的信号量 */
static rt_device_t can_dev;            /* CAN 设备句柄 */

/* 新增全局变量，供 OLED 显示线程读取 */
int32_t g_can_current_ma = 0;

/* 接收数据回调函数 */
static rt_err_t can_rx_call(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(&rx_sem);
    return RT_EOK;
}

/* CAN 接收与解析线程 */
static void can_rx_thread(void *parameter)
{
    struct rt_can_msg rxmsg = {0};

    rt_device_set_rx_indicate(can_dev, can_rx_call);

    while (1)
    {
        rxmsg.hdr = -1;
        rt_sem_take(&rx_sem, RT_WAITING_FOREVER);
        rt_device_read(can_dev, 0, &rxmsg, sizeof(rxmsg));

        if (rxmsg.id == TARGET_CAN_ID && rxmsg.len >= 5)
        {
            uint32_t raw_value = ((uint32_t)rxmsg.data[0] << 24) |
                                 ((uint32_t)rxmsg.data[1] << 16) |
                                 ((uint32_t)rxmsg.data[2] << 8)  |
                                 ((uint32_t)rxmsg.data[3]);

            int32_t current_mA = (int32_t)(raw_value - 0x80000000);
            uint8_t error_flag = rxmsg.data[4];

            if (error_flag == 0)
            {
                /* 正常情况：更新到全局变量供 OLED 读取 */
                g_can_current_ma = current_mA;
                /* 保留后台打印用于调试 */
                rt_kprintf("[CAN] ID:0x%03X | Current: %d mA\n", rxmsg.id, current_mA);
            }
            else
            {
                rt_kprintf("[CAN] ID:0x%03X | Sensor Error! Code: 0x%02X\n", rxmsg.id, error_flag);
            }
        }
    }
}

/* 自动初始化并启动 CAN 接收 */
int can_app_init(void)
{
    rt_err_t res;
    rt_thread_t thread;

    can_dev = rt_device_find(CAN_DEV_NAME);
    if (!can_dev)
    {
        rt_kprintf("find %s failed!\n", CAN_DEV_NAME);
        return RT_ERROR;
    }

    res = rt_device_control(can_dev, RT_CAN_CMD_SET_BAUD, (void *)CAN250kBaud);
    if (res != RT_EOK) return RT_ERROR;

    struct rt_can_filter_item filter_item = {TARGET_CAN_ID, 0, 0, 1, 0x7FF, RT_NULL, RT_NULL};
    struct rt_can_filter_config filter_cfg = {1, 1, &filter_item};
    rt_device_control(can_dev, RT_CAN_CMD_SET_FILTER, &filter_cfg);

    rt_sem_init(&rx_sem, "rx_sem", 0, RT_IPC_FLAG_FIFO);

    res = rt_device_open(can_dev, RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
    if (res != RT_EOK) return res;

    thread = rt_thread_create("can_rx", can_rx_thread, RT_NULL, 1024, 25, 10);
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
        return RT_EOK;
    }
    return RT_ERROR;
}
/* 导出到自动初始化环节 */
INIT_APP_EXPORT(can_app_init);
