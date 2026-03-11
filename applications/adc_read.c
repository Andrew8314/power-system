#include <rtthread.h>
#include <rtdevice.h>

#define ADC_DEV_NAME        "adc1"      /* 驱动注册的设备名 */
#define ADC_DEV_CHANNEL     0           /* 对应硬件的通道号PA0 */
#define REFER_VOLTAGE       3300        /* 参考电压 3.3V，单位：mV */
#define CONVERT_BITS        4096        /* 12位 ADC 的分度值 (2^12) */

/* 新增全局变量，供 OLED 显示线程读取 */
uint32_t g_adc_voltage_mv = 0;

static void adc_read_entry(void *parameter)
{
    rt_adc_device_t adc_dev;
    rt_uint32_t value, vol_mv;

    /* 1. 查找设备 */
    adc_dev = (rt_adc_device_t)rt_device_find(ADC_DEV_NAME);
    if (adc_dev == RT_NULL) {
        rt_kprintf("Cannot find %s device!\n", ADC_DEV_NAME);
        return;
    }

    /* 2. 使能通道 */
    rt_adc_enable(adc_dev, ADC_DEV_CHANNEL);

    /* 3. 循环读取 */
    while (1)
    {
        value = rt_adc_read(adc_dev, ADC_DEV_CHANNEL);
        vol_mv = value * REFER_VOLTAGE / CONVERT_BITS;

        /* 更新到全局变量供 OLED 读取 */
        g_adc_voltage_mv = vol_mv;

        /* 延时 1000ms 读取一次 */
        rt_thread_mdelay(1000);
    }
}

/* 初始化并启动 ADC 采集线程 */
int adc_read_init(void)
{
    rt_thread_t tid;

    tid = rt_thread_create("adc_read", adc_read_entry, RT_NULL, 1024, 22, 10);
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
        return RT_EOK;
    }
    return -RT_ERROR;
}
/* 导出到自动初始化，开机即运行 */
INIT_APP_EXPORT(adc_read_init);
