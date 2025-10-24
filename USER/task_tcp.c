#include "task_tcp.h"

#include <stdio.h>
#include "wiz_platform.h"
#include "wizchip_conf.h"
#include "wiz_interface.h"
#include "tcp_server_multi_socket.h"

#include "spi.h"
#include "modtcpbsp.h"

#include "mb.h"
#include "mbport.h"
#include "mb_user_reg.h"  /* in modbus/reg/ */

/*
 * task_tcp.c - 主任务入口（网络初始化与 Modbus 主循环）
 *
 * 说明：
 * - 本文件演示如何初始化 W5500、启动单连接的 Modbus/TCP 服务并进入
 *   主循环：
 *     1) 调用 tcp_srv_single_init() / tcp_srv_single_set_keepalive()
 *     2) 初始化 FreeModbus（eMBTCPInit / eMBEnable）
 *     3) 在循环中调用 vMBPortTCPPool() 驱动 TCP 数据读取与组帧，
 *        然后调用 eMBPoll() 处理 Modbus 请求。
 * - 日志级别由 `HARDWARE/MOD_TCP_BSP/modtcpbsp.h` 中的 `MODTCP_DEBUG_LEVEL`
 *   控制。把它设为 1 输出基本事件；设为 2 会输出更详细的周期性状态和
 *   十六进制帧转储（仅用于调试）。
 *
 * 切换单/多连接：
 * - 若需要切换为多连接，请编辑 `modtcpbsp.h`，去掉 `#define TCP_MULTI_CONNECTION_MODE`
 *   前的注释并重新编译。运行时不支持动态切换（当前实现为编译时选择）。
 */

void vMBPortTCPPool(void);
//TCP任务

OS_STK TCP_TASK_STK[TCP_STK_SIZE];
OS_STK LED1_TASK_STK[LED1_STK_SIZE];


#define MODBUS_PORT  502
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)
/* network information */
wiz_NetInfo default_net_info = {
    {0x00, 0x08, 0xdc, 0x12, 0x22, 0x12},   /* mac */
    {192, 168, 103, 200},                   /* ip  */
    {255, 255, 255, 0},                     /* sn  */
    {192, 168, 103, 1},                     /* gw  */
    {8, 8, 8, 8},                           /* dns */
    NETINFO_STATIC                          /* dhcp */
};
uint8_t ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};
uint16_t local_port = 502;


void tcp_task(void *pdata)
{
    eMBErrorCode eStatus;
    static uint32_t last_debug_time = 0;
    static uint32_t poll_count = 0;

    uint16_t rx_rsr;
    uint16_t rx_rd;
    uint32_t current_time;

    /* --- 初始化部分 --- */
    wizchip_reset();
    delay_ms(10);
    wizchip_initialize();
    delay_ms(10);
    network_init(ethernet_buf, &default_net_info);
    delay_ms(10);
    SPI2_SetSpeed(SPI_BaudRatePrescaler_8);

    tcp_srv_single_init(MODBUS_PORT);               
    tcp_srv_single_set_keepalive(10);

    /* 初始化用户寄存器（LED控制位） */
    vMBUserRegInit();

    /* 初始化并配置 Modbus TCP */
    if ((eStatus = eMBTCPInit(MODBUS_PORT)) != MB_ENOERR) {
        printf("[ERR] Modbus TCP init failed: %d\r\n", eStatus);
        while (1) { delay_ms(1000); }
    }

    /* 设置功能码掩码（启用03和06功能码） */
    if ((eStatus = eMBSetSlaveID(0x01, TRUE, NULL, 0)) != MB_ENOERR) {
        printf("[ERR] Set slave ID failed: %d\r\n", eStatus);
        while (1) { delay_ms(1000); }
    }

    /* 启用所有支持的功能码 */
    if ((eStatus = eMBEnable()) != MB_ENOERR) {
        printf("[ERR] Modbus enable failed: %d\r\n", eStatus);
        while (1) { delay_ms(1000); }
    }

    /* 主循环 */
    for (;;)
    {
        poll_count++;

        /* 先主动推进一次 TCP，确保新数据被读取 */
        vMBPortTCPPool();
        eStatus = eMBPoll();
        delay_ms(1);
    }
}



//LED1任务
void led1_task(void *pdata)
{	  
	while(1)
	{
		LED1=0;
		delay_ms(1500);
		LED1=1;
		delay_ms(1500);
	};
}







