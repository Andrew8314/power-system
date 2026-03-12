/*
 * File      : udisk_logger.c
 * 说明      :  U盘本地存储线程（按天自动分片）
 */

#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h> /* 引入 stat，用于探测目录是否存在 */
#include <dfs_fs.h>  /* 新增：引入文件系统挂载 API */

#define DBG_TAG "app.logger"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

/* ================= 引入外部传感器全局变量 ================= */
extern float g_aht10_temp;
extern float g_aht10_humi;
extern uint32_t g_adc_voltage_mv;
extern int32_t g_ds18b20_temp;
extern int32_t g_can_current_ma;

/* 挂载根路径，如果挂载在指定文件夹，可改为 "/udisk" 等 */
#define UDISK_ROOT_PATH ""

static void udisk_logger_entry(void *parameter)
{
    int fd;
    char current_file_path[64];
    char write_buf[128];
    time_t now;
    struct tm *tm_info;
    struct stat buffer; /* 用于存放探测状态 */

    rt_thread_mdelay(5000);

    while (1)
    {
        /* 每次写操作前，先探测根目录是否存在（检查 U 盘是否在线） */
        /* 如果 U 盘掉线，stat 会返回非 0 值，我们直接跳过本次写入，保护线程不崩溃 */
        /* 1. 探测根目录是否存在（检查文件系统是否已挂载） */
        if (stat("/", &buffer) != 0)
        {
            LOG_W("Filesystem not found on '/', attempting to mount U-disk...");

            /* 尝试主动挂载文件系统
             * RT-Thread 通常将 U 盘的块设备命名为 "udisk" 或 "udisk0"
             * "elm" 代表 FatFS 文件系统格式
             */
            if (dfs_mount("ud0-0", "/", "elm", 0, 0) == 0)
            {
                LOG_I("Mounted 'udisk0' to '/' successfully!");
            }
            else if (dfs_mount("udisk", "/", "elm", 0, 0) == 0)
            {
                LOG_I("Mounted 'udisk' to '/' successfully!");
            }
            else
            {
                LOG_E("Mount failed. Please plug the U-disk again...");
                rt_thread_mdelay(2000);
                continue; /* 挂载失败，说明 U 盘真没插好，延时后重试 */
            }
        }

        now = time(RT_NULL);
        tm_info = localtime(&now);

        snprintf(current_file_path, sizeof(current_file_path), "/log_%04d%02d%02d.csv",
                tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday);

        /* 使用 access 判断文件是否存在，更加安全 */
        int is_new_file = (stat(current_file_path, &buffer) != 0) ? 1 : 0;
        /* 打开文件准备写入 */
        fd = open(current_file_path, O_WRONLY | O_CREAT | O_APPEND);
        if (fd >= 0)
        {
            if (is_new_file)
            {
                const char *header = "Time,AHT10_T(C),AHT10_H(%),DS18B20(C),ADC_V(V),CAN_I(mA)\n";
                write(fd, header, strlen(header));
            }

            memset(write_buf, 0, sizeof(write_buf));
            snprintf(write_buf, sizeof(write_buf), "%04d-%02d-%02d %02d:%02d:%02d,%.2f,%.2f,%.2f,%.3f,%d\n",
                    tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                    tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
                    g_aht10_temp, g_aht10_humi, (float)g_ds18b20_temp / 10.0f,
                    (float)g_adc_voltage_mv / 1000.0f, (int)g_can_current_ma);

            write(fd, write_buf, strlen(write_buf));
            close(fd);
        }

        rt_thread_mdelay(5000);
    }
}

static int udisk_logger_init(void)
{
    rt_thread_t tid;
    tid = rt_thread_create("udisk_log", udisk_logger_entry, RT_NULL, 2048, 23, 10);
    if (tid != RT_NULL)
    {
        rt_thread_startup(tid);
        return RT_EOK;
    }
    return -RT_ERROR;
}
INIT_APP_EXPORT(udisk_logger_init);
