#include <rtthread.h>
#include <rtdevice.h>

#define ADC_DEV_NAME        "adc1"      /* 驱动注册的设备名 */
#define ADC_DEV_CHANNEL     0           /* 对应硬件的通道号PA0 */
#define REFER_VOLTAGE       3300        /* 参考电压 3.3V，单位：mV */
#define CONVERT_BITS        4096        /* 12位 ADC 的分度值 (2^12) */

extern float global_voltage;
extern rt_mutex_t data_mutex;

void update_adc_voltage(void)
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

    /* 3. 读取采样值 (获取原始 12位 数字) */
    value = rt_adc_read(adc_dev, ADC_DEV_CHANNEL);
    rt_kprintf("Raw Value: %d\n", value);

    /* 4. 转换为实际电压值 (单位: mV) */
    vol_mv = value * REFER_VOLTAGE / CONVERT_BITS;

    /* 打印真实电压，格式为 X.XXX V (巧妙避开 %f 浮点打印) */
    rt_kprintf("Voltage: %d.%03d V\n", vol_mv / 1000, vol_mv % 1000);

    /* Update global */
    rt_mutex_take(data_mutex, RT_WAITING_FOREVER);
    global_voltage = vol_mv / 1000.0;
    rt_mutex_release(data_mutex);

    /* 5. 关闭通道 (省电) */
    rt_adc_disable(adc_dev, ADC_DEV_CHANNEL);
}

MSH_CMD_EXPORT(update_adc_voltage, update adc1 channel 0 voltage);
