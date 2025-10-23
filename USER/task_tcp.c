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
#include "mb_user_reg.h"

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

//TCP任务
/*


void tcp_task(void *pdata)
{
	printf("%s TCP Server Multi Socket example\r\n",_WIZCHIP_ID_);
	wizchip_reset();
	delay_ms(10);
	//wizchip init 
	wizchip_initialize();
	delay_ms(10);
	network_init(ethernet_buf, &default_net_info);
	delay_ms(10);
	SPI2_SetSpeed(8);//提速
	while(1)
	{
		multi_tcps_socket(ethernet_buf, local_port);
	};
}
*/

static uint8_t rxbuf[TCP_RX_MAX];


void tcp_task(void *pdata)
{
    eMBErrorCode eStatus;
    static uint32_t last_debug_time = 0;
    static uint32_t poll_count = 0;
    static uint32_t last_print_time = 0;  /* 用于每秒打印数据 */

    uint16_t rx_rsr;
    uint16_t rx_rd;
    uint32_t current_time;
    const uint8_t *pbuf;
    uint16_t plen;
    int i;

    printf("W5500 Modbus/TCP Server start\r\n");

    /* --- 初始化部分保持不变 --- */
    wizchip_reset();
    delay_ms(10);
    wizchip_initialize();
    delay_ms(10);
    network_init(ethernet_buf, &default_net_info);
    delay_ms(10);
    SPI2_SetSpeed(SPI_BaudRatePrescaler_8);

    tcp_srv_single_init(MODBUS_PORT);               
    tcp_srv_single_set_keepalive(10);
    printf("[TCP] Listening on port %u\r\n", MODBUS_PORT);

    if (eMBTCPInit(MODBUS_PORT) != MB_ENOERR) {
        printf("[MB] eMBTCPInit failed!\r\n");
        while (1) { delay_ms(1000); }
    }

    if (eMBEnable() != MB_ENOERR) {
        printf("[MB] eMBEnable failed!\r\n");
        while (1) { delay_ms(1000); }
    }
    printf("[MB] Modbus slave ready on %u\r\n", MODBUS_PORT);

    /* 主循环 */
    for (;;)
    {
        poll_count++;

    /* 驱动TCP层（被动拉模式）：负责读硬件 socket、append 到累加缓冲并组帧 */
    vMBPortTCPPool();

        /* 处理Modbus请求 */
        eStatus = eMBPoll();

        /* 每秒输出一次接收到的数据 */
        current_time = OSTimeGet();
        /* 每2秒输出一次详细状态 */
        if (current_time - last_debug_time > 1000) {
            last_debug_time = current_time;
            
            printf("[DEBUG] === Modbus TCP Status ===\r\n");
            printf("[DEBUG] Poll count: %lu, eMBPoll result: %d\r\n", poll_count, eStatus);
            printf("[DEBUG] TCP Connected: %s, RX Buffer: %u/%u bytes\r\n", 
                   tcp_srv_single_is_connected() ? "Yes" : "No",
                   tcp_srv_single_get_rx_buffer_usage(),
                   tcp_srv_single_get_rx_buffer_size());
            
            /* 输出TCP socket状态 */
            printf("[DEBUG] Socket state: ");
            switch(getSn_SR(0)) {
                case SOCK_CLOSED: printf("CLOSED"); break;
                case SOCK_INIT: printf("INIT"); break;
                case SOCK_LISTEN: printf("LISTEN"); break;
                case SOCK_ESTABLISHED: printf("ESTABLISHED"); break;
                case SOCK_CLOSE_WAIT: printf("CLOSE_WAIT"); break;
                default: printf("UNKNOWN(%u)", getSn_SR(0)); break;
            }
            printf("\r\n");
            
            /* 输出TCP接收缓冲区状态 */
            rx_rsr = getSn_RX_RSR(0);
            rx_rd = getSn_RX_RD(0);
            printf("[DEBUG] TCP HW Buffer: RSR=%u, RD=%u\r\n", rx_rsr, rx_rd);
            printf("[DEBUG] ============================\r\n");
        }

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







