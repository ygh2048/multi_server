#include "task_tcp.h"

#include <stdio.h>
#include "wiz_platform.h"
#include "wizchip_conf.h"
#include "wiz_interface.h"
#include "tcp_server_multi_socket.h"

#include "spi.h"

OS_STK TCP_TASK_STK[TCP_STK_SIZE];
OS_STK LED1_TASK_STK[LED1_STK_SIZE];

#define ETHERNET_BUF_MAX_SIZE (1024 * 2)
/* network information */
wiz_NetInfo default_net_info = {
    {0x00, 0x08, 0xdc, 0x12, 0x22, 0x12},   // mac
    {192, 168, 103, 200},                   // ip
    {192, 168, 103, 1},                     // gw
    {255, 255, 255, 0},                     // sn
    {8, 8, 8, 8},                           // dns
    NETINFO_DHCP    
}; 
uint8_t ethernet_buf[ETHERNET_BUF_MAX_SIZE] = {0};
uint16_t local_port = 502;

//TCP任务
void tcp_task(void *pdata)
{
	printf("%s TCP Server Multi Socket example\r\n",_WIZCHIP_ID_);
	wizchip_reset();
	delay_ms(10);
	/* wizchip init */
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

//LED1任务
void led1_task(void *pdata)
{	  
	while(1)
	{
		LED1=0;
		delay_ms(300);
		LED1=1;
		delay_ms(300);
	};
}







