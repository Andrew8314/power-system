/*
 * File      : rs485_app.c
 * 说明      : RS485 传感器数据定时发送线程
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "rs485.h"
#include <stdio.h>
#include <string.h>

#define DBG_TAG "app.rs485"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* ================= 硬件参数配置 ================= */
#define APP_RS485_SERIAL       "uart6"         // 串口设备名称，请根据实际情况修改
#define APP_RS485_BAUDRATE     9600            // 波特率
#define APP_RS485_PARITY       0               // 校验位 0:无校验
#define APP_RS485_PIN          104             // 收发控制引脚，如果是自动收发模块填 -1，手动填类似 GET_PIN(A, 8)
#define APP_RS485_LVL          1               // 控制引脚高电平发送

/* ================= 引入外部传感器数据 ================= */
extern float g_aht10_temp;
extern float g_aht10_humi;
extern int32_t g_ds18b20_temp;
extern uint32_t g_adc_voltage_mv;
/* 新增：引入 CAN 接收线程中定义的电流全局变量 */
extern int32_t g_can_current_ma;

/* ================= 发送线程 ================= */
static void rs485_send_thread_entry(void *parameter)
{
    char send_buf[128]; // 数据拼接缓存区（128字节目前足够容纳新增的数据）

    /* 1. 动态创建 RS485 实例 */
    rs485_inst_t *hinst = rs485_create(APP_RS485_SERIAL, APP_RS485_BAUDRATE,
            APP_RS485_PARITY, APP_RS485_PIN, APP_RS485_LVL);

    if (hinst == RT_NULL)
    {
        LOG_E("RS485 instance create failed!");
        return;
    }

    /* 2. 打开 RS485 连接 */
    if (rs485_connect(hinst) != RT_EOK)
    {
        LOG_E("RS485 connect failed!");
        rs485_destory(hinst);
        return;
    }

    LOG_I("RS485 init success, start sending sensor data...");

    /* 3. 循环打包并发送数据 */
    while (1)
    {
        memset(send_buf, 0, sizeof(send_buf));

        /* * 修改点：在 JSON 字符串末尾增加了 "CAN_I_mA":%d 字段
         * 由于 g_can_current_ma 是 int32_t 类型整数，所以这里用 %d 打印
         */
        snprintf(send_buf, sizeof(send_buf),
                "{\"AHT10_T\":%.2f, \"AHT10_H\":%.2f, \"DS18B20\":%.2f, \"ADC_V\":%.2f, \"CAN_I_mA\":%d}\r\n",
                g_aht10_temp,
                g_aht10_humi,
                (float)g_ds18b20_temp / 10.0f,
                (float)g_adc_voltage_mv / 1000.0f,
                (int)g_can_current_ma);            // 将 CAN 读取的毫安值直接传入

        /* 4. 调用底层发送接口将拼接好的字符串发出去 */
        int send_len = rs485_send(hinst, (void *)send_buf, strlen(send_buf));

        if (send_len > 0)
        {
           // LOG_D("Sent %d bytes: %s", send_len, send_buf);
        }
        else
        {
            LOG_E("RS485 Send failed.");
        }

        /* 每隔 2 秒发送一次 */
        rt_thread_mdelay(2000);
    }
}

/* ================= 线程初始化 ================= */
static int rs485_app_init(void)
{
    rt_thread_t tid;

    /* 创建 RS485 数据发送线程 */
    tid = rt_thread_create("rs485_tx",
            rs485_send_thread_entry,
            RT_NULL,
            2048,
            15,
            10);

    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
        return RT_EOK;
    }

    LOG_E("Failed to create RS485 thread.");
    return -RT_ERROR;
}

/* 导出到自动初始化机制中，系统启动后自动运行 */
INIT_APP_EXPORT(rs485_app_init);
