/*
 * File      : rs485_app.c
 * 说明      : RS485 传感器数据命令查询模式（问答式/主从模式）
 * Send 'START', 'STOP', or 'GET'
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "rs485.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define DBG_TAG "app.rs485"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* ================= 硬件参数配置 ================= */
#define APP_RS485_SERIAL       "uart6"
#define APP_RS485_BAUDRATE     9600
#define APP_RS485_PARITY       0
#define APP_RS485_PIN          104
#define APP_RS485_LVL          1

/* ================= 引入外部传感器数据 ================= */
//extern float g_aht10_temp;
//extern float g_aht10_humi;
extern int32_t g_ds18b20_temp;
extern uint32_t g_adc_voltage_mv;
extern int32_t g_can_current_ma;
extern uint32_t g_adc_voltage_mv_ch1;
extern uint8_t g_sys_alarm_status; /* 引入全局报警状态 */

/* ================= 收发线程 ================= */
static void rs485_trx_thread_entry(void *parameter)
{
    char send_buf[256];
    char recv_buf[64];
    time_t now;
    struct tm *tm_info;

    uint8_t is_continuous_mode = 0; /* 状态标志位：0=停止/轮询模式，1=连续自动上报模式 */
    uint8_t last_alarm_status = 0;  /* 记录上一次的报警状态，用于边缘触发 */
    uint32_t last_send_tick = rt_tick_get(); /* 记录上次连续发送的时间戳 */

    /* 1. 初始化 RS485 实例 */
    rs485_inst_t *hinst = rs485_create(APP_RS485_SERIAL, APP_RS485_BAUDRATE,
            APP_RS485_PARITY, APP_RS485_PIN, APP_RS485_LVL);
    if (hinst == RT_NULL) return;
    if (rs485_connect(hinst) != RT_EOK) { rs485_destory(hinst); return; }

    LOG_I("RS485 ready. Send 'START', 'STOP', or 'GET'. Auto-report ON.");

    while (1)
    {
        memset(recv_buf, 0, sizeof(recv_buf));
        uint8_t need_send = 0;

        /* 将接收超时时间固定为 500 毫秒，确保线程能频繁醒来检查报警状态 */
        rs485_set_recv_tmo(hinst, 500);

        /* 阻塞接收，参数只有 3 个 */
        int recv_len = rs485_recv(hinst, recv_buf, sizeof(recv_buf) - 1);

        if (recv_len > 0)
        {
            /* 收到上位机指令，进行解析 */
            if (strstr(recv_buf, "START") != RT_NULL)
            {
                is_continuous_mode = 1;
                need_send = 1; /* 收到开始指令，立刻回一包 */
                LOG_I("Mode: Continuous Streaming ON");
            }
            else if (strstr(recv_buf, "STOP") != RT_NULL)
            {
                is_continuous_mode = 0;
                LOG_I("Mode: Continuous Streaming OFF");
            }
            else if (strstr(recv_buf, "GET") != RT_NULL)
            {
                need_send = 1; /* 单次获取 */
            }
        }
        /* ---------------- 第二步：检测异常主动上报 ---------------- */
        /* 如果状态从 0(正常) 变为 非0(异常)，触发主动推送 */
        if (g_sys_alarm_status != 0 && last_alarm_status == 0)
        {
            need_send = 1;
            LOG_W("Proactive RS485 report triggered by ALARM!");
        }
        last_alarm_status = g_sys_alarm_status; /* 更新报警状态历史 */

        /* 2. 判断当前是否需要持续发送，以及设定发送间隔 */
        uint32_t current_interval = 0;
        if (g_sys_alarm_status != 0)
        {
            current_interval = 1000; /* 报警期间：每 1 秒狂发一次，提醒上位机 */
        }
        else if (is_continuous_mode)
        {
            current_interval = 2000; /* 正常连续模式(START指令开启)：每 2 秒发一次 */
        }

        /* 3. 如果当前处于需要连续发送的状态，检查时间是否到了 */
        if (current_interval > 0 && need_send == 0)
        {
            if ((rt_tick_get() - last_send_tick) >= rt_tick_from_millisecond(current_interval))
            {
                need_send = 1;
            }
        }

        /* 统一的打包发送流程 */
        if (need_send)
        {
            memset(send_buf, 0, sizeof(send_buf));
            now = time(RT_NULL);
            tm_info = localtime(&now);

            snprintf(send_buf, sizeof(send_buf),
                    "{\"Time\":\"%04d-%02d-%02d %02d:%02d:%02d\", \"DS18B20\":%.2f, \"ADC_V0\":%.2f, \"ADC_V1\":%.2f, \"CAN_I_mA\":%d, \"Alarm_Code\":%d}\r\n",
                    tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                    tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
                    (float)g_ds18b20_temp / 10.0f,
                    (float)g_adc_voltage_mv / 1000.0f,
                    (float)g_adc_voltage_mv_ch1 / 1000.0f,
                    (int)g_can_current_ma,
                    (int)g_sys_alarm_status);

            rs485_send(hinst, (void *)send_buf, strlen(send_buf));
            /* 更新最后一次发送的时间戳 */
            last_send_tick = rt_tick_get();
        }
    }
}

/* ================= 线程初始化 ================= */
static int rs485_app_init(void)
{
    rt_thread_t tid;

    /* 将线程名字改为 trx (transmit & receive) 以符合其新的身份 */
    tid = rt_thread_create("rs485_trx", rs485_trx_thread_entry, RT_NULL, 2048, 15, 10);

    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
        return RT_EOK;
    }

    LOG_E("Failed to create RS485 thread.");
    return -RT_ERROR;
}
INIT_APP_EXPORT(rs485_app_init);
