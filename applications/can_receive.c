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


/* 接收数据回调函数 */
static rt_err_t can_rx_call(rt_device_t dev, rt_size_t size)
{
    /* CAN 接收到数据后产生中断，调用此回调函数，释放接收信号量 */
    rt_sem_release(&rx_sem);
    return RT_EOK;
}

/* CAN 接收与解析线程 */
static void can_rx_thread(void *parameter)
{
    struct rt_can_msg rxmsg = {0};

    /* 设置接收回调函数 */
    rt_device_set_rx_indicate(can_dev, can_rx_call);

    while (1)
    {
        /* hdr 值为 -1，表示直接从 uselist 链表读取数据 */
        rxmsg.hdr = -1;

        /* 阻塞等待接收信号量 */
        rt_sem_take(&rx_sem, RT_WAITING_FOREVER);

        /* 从 CAN 读取一帧数据 */
        rt_device_read(can_dev, 0, &rxmsg, sizeof(rxmsg));

        /* 校验 ID 且数据长度至少包含电流(4字节)和错误位(1字节) */
        if (rxmsg.id == TARGET_CAN_ID && rxmsg.len >= 5)
        {
            /* 1. 大端模式：将前 4 个字节(Byte 0~3)拼成 32 位无符号整数 */
            uint32_t raw_value = ((uint32_t)rxmsg.data[0] << 24) |
                                 ((uint32_t)rxmsg.data[1] << 16) |
                                 ((uint32_t)rxmsg.data[2] << 8)  |
                                 ((uint32_t)rxmsg.data[3]);

            /* 2. 计算电流：减去偏移量 0x80000000 */
            int32_t current_mA = (int32_t)(raw_value - 0x80000000);

            /* 3. 提取错误指示标志 (Byte 4) */
            uint8_t error_flag = rxmsg.data[4];

            /* 4. 打印解析结果 */
            if (error_flag == 0)
            {
                /* 正常情况打印电流值 */
                rt_kprintf("[CAN] ID:0x%03X | Current: %d mA\n", rxmsg.id, current_mA);
            }
            else
            {
                /* 传感器上报失效或错误 */
                rt_kprintf("[CAN] ID:0x%03X | Sensor Error! Error Code: 0x%02X\n", rxmsg.id, error_flag);
            }
            // Update global data
            extern int32_t global_current_ma;
            extern uint8_t global_error_flag;
            extern rt_mutex_t data_mutex;
            rt_mutex_take(data_mutex, RT_WAITING_FOREVER);
            global_current_ma = current_mA;
            global_error_flag = error_flag;
            rt_mutex_release(data_mutex);
        }
    }
}

int can_sample_receive(int argc, char *argv[])
{
    rt_err_t res;
    rt_thread_t thread;
    char can_name[RT_NAME_MAX];

    if (argc == 2)
    {
        rt_strncpy(can_name, argv[1], RT_NAME_MAX);
    }
    else
    {
        rt_strncpy(can_name, CAN_DEV_NAME, RT_NAME_MAX);
    }

    /* 1. 查找 CAN 设备 */
    can_dev = rt_device_find(can_name);
    if (!can_dev)
    {
        rt_kprintf("find %s failed!\n", can_name);
        return RT_ERROR;
    }

    /* =================== 核心配置区 =================== */
    /* 2. 设置 CAN 波特率为 250kbps (严格按照传感器说明书) */
    res = rt_device_control(can_dev, RT_CAN_CMD_SET_BAUD, (void *)CAN250kBaud);
    if (res != RT_EOK)
    {
        rt_kprintf("Set CAN baud rate 250k failed!\n");
        return RT_ERROR;
    }

    /* 3. 配置硬件过滤器 (只接收 ID 为 0x3C2 的标准帧数据，屏蔽总线其他无关数据) */
    struct rt_can_filter_item filter_item =
    {
        TARGET_CAN_ID, 0, 0, 1, 0x7FF, RT_NULL, RT_NULL
        /* 参数解释：ID, 标准帧(0), 数据帧(0), 掩码模式(1), 掩码0x7FF全匹配, 回调空, 回调参数空 */
    };
    struct rt_can_filter_config filter_cfg = {1, 1, &filter_item}; /* 过滤项数量为 1，激活过滤器 */

    res = rt_device_control(can_dev, RT_CAN_CMD_SET_FILTER, &filter_cfg);
    if (res != RT_EOK)
    {
        rt_kprintf("Set CAN hardware filter failed!\n");
        /* 注意：如果有的芯片底层不支持硬件过滤，这里报错可以注释掉这部分代码，不影响软件过滤 */
    }
    /* =================================================== */

    /* 初始化 CAN 接收信号量 */
    rt_sem_init(&rx_sem, "rx_sem", 0, RT_IPC_FLAG_FIFO);

    /* 以中断接收方式打开 CAN 设备 */
    res = rt_device_open(can_dev, RT_DEVICE_FLAG_INT_TX | RT_DEVICE_FLAG_INT_RX);
    RT_ASSERT(res == RT_EOK);

    /* 创建数据接收线程 */
    thread = rt_thread_create("can_rx", can_rx_thread, RT_NULL, 1024, 25, 10);
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
        rt_kprintf("CAN receive thread started successfully.\n");
    }
    else
    {
        rt_kprintf("create can_rx thread failed!\n");
    }
    return res;
}
/* 导出到 msh 命令列表中 */
MSH_CMD_EXPORT(can_sample_receive, CAN device receive sample);


