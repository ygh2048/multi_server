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

void vMBPortTCPPool(void);


OS_STK TCP_TASK_STK[TCP_STK_SIZE];
OS_STK LED1_TASK_STK[LED1_STK_SIZE];



#define MODBUS_PORT  502
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)
/* network information */
wiz_NetInfo default_net_info = {
    {0x00, 0x08, 0xdc, 0x12, 0x22, 0x12},   // mac
    {192, 168, 103, 200},                   // ip
    {192, 168, 103, 1},                     // gw
    {255, 255, 255, 0},                     // sn
    {8, 8, 8, 8},                           // dns
    NETINFO_STATIC                          // DHCP设置   NETINFO_DHCP    
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
    printf("%s Modbus/TCP Server start\r\n", _WIZCHIP_ID_);

    /* --- W5500 + 网络初始化 --- */
    wizchip_reset();
    delay_ms(10);
    wizchip_initialize();
    delay_ms(10);
    network_init(ethernet_buf, &default_net_info);
    delay_ms(10);

    /* SPI 提速 */
    SPI2_SetSpeed(SPI_BaudRatePrescaler_8);  // 或者 _32 看线长/连线质量

    /* --- 打开多 socket 共同监听 502 --- */
    tcp_srv_init(MODBUS_PORT);               
    tcp_srv_set_keepalive(10);                // 设置 Keepalive，约10秒
    printf("[TCP] Listening on port %u\r\n", MODBUS_PORT);

    /* --- FreeModbus 初始化 --- */
    if (eMBTCPInit(MODBUS_PORT) != MB_ENOERR) {
        printf("[MB] eMBTCPInit failed!\r\n");
        while (1) { delay_ms(1000); }
    }

    if (eMBEnable() != MB_ENOERR) {
        printf("[MB] eMBEnable failed!\r\n");
        while (1) { delay_ms(1000); }
    }
    printf("[MB] Modbus slave ready on %u\r\n", MODBUS_PORT);

    for (;;)
    {
        /* 1) 先让以太网/TCP 层跑起来，把收到的 TCP 帧喂给 FreeModbus */
        tcp_srv_poll();  // 调用底层的 TCP 轮询函数

        /* 2) 轮询 Modbus 协议栈，处理 Modbus 请求 */
        (void)eMBPoll(); // 处理 Modbus 请求

        /* 3) （可选）你可以周期性更新输入寄存器/保持寄存器等 */
        // 例如：MBUSR_SetInputByAddr(1, some_sensor_value);

        delay_ms(1); // 1ms 的延时（控制轮询频率）
    }
}




//LED1任务
void led1_task(void *pdata)
{	  
	while(1)
	{
		LED1=0;
		delay_ms(1300);
		LED1=1;
		delay_ms(1300);
	};
}







