/**
  ******************************************************************************
  * @file    main.c
  * @author  WIZnet Software Team
  * @version V1.0
  * @date    2015-02-14
  * @brief   W5500模块下TCPServer
  ******************************************************************************
  * @attention
  *
  * 实验平台:野火  STM32 F407 开发板  
  * 论坛    :http://www.firebbs.cn
  * 淘宝    :https://fire-stm32.taobao.com
  *
  ******************************************************************************
  */
 
#include <string.h> 
#include "stm32f4xx.h"
#include "./usart/bsp_usart_debug.h"

#include "delay.h"
#include "spi.h"
#include "socket.h"	
#include "w5500_conf.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define SOCK_TCPS        0
#define DATA_BUF_SIZE   2048
/* Private macro -------------------------------------------------------------*/
uint8_t gDATABUF[DATA_BUF_SIZE];
// 默认网络配置
wiz_NetInfo gWIZNETINFO = { .mac = {0x00, 0x08, 0xdc,0x11, 0x11, 0x11},
                            .ip = {192, 168, 103, 202},
                            .sn = {255,255,255,0},
                            .gw = {192, 168, 103, 1},
                            .dns = {8,8,8,8},
                            .dhcp = NETINFO_STATIC };
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
void platform_init(void);								// 初始化相关的主机外设
void network_init(void);								// 初始化网络信息并显示

/****************************************************
函数名：		main
形参：		无
返回值：		无
函数功能：	主函数
****************************************************/
int main(void)
{
	uint8_t tmp;

	uint16_t len=0;
	uint8_t memsize[2][8] = {{2,2,2,2,2,2,2,2},{2,2,2,2,2,2,2,2}};
//	uint8_t DstIP[4]={192,168,103,19};  //目的IP地址
//	uint16_t	DstPort=6000;           //端口号
	//初始化与主机相关的外设
	platform_init();
	// 首先，应该注册用户为访问WIZCHIP实现的SPI回调函数 
	/* 临界区回调 */
	reg_wizchip_cris_cbfunc(SPI_CrisEnter, SPI_CrisExit);	//注册临界区函数
	/* 片选回调 */
#if   _WIZCHIP_IO_MODE_ == _WIZCHIP_IO_MODE_SPI_VDM_
	reg_wizchip_cs_cbfunc(SPI_CS_Select, SPI_CS_Deselect);  //注册SPI片选信号函数
#elif _WIZCHIP_IO_MODE_ == _WIZCHIP_IO_MODE_SPI_FDM_
	reg_wizchip_cs_cbfunc(SPI_CS_Select, SPI_CS_Deselect);  // CS must be tried with LOW.
#else
   #if (_WIZCHIP_IO_MODE_ & _WIZCHIP_IO_MODE_SIP_) != _WIZCHIP_IO_MODE_SIP_
      #error "Unknown _WIZCHIP_IO_MODE_"
   #else
      reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
   #endif
#endif
	/* SPI读和写回调函数 */
	reg_wizchip_spi_cbfunc(SPI_ReadByte, SPI_WriteByte);	//注册读写函数
    printf("初始化 Demo V1.0 \r\n");	
    gpio_for_w5500_config();						        /*初始化MCU相关引脚*/
    reset_w5500();											/*硬复位W5500*/

	 printf("reset_w5500() over \r\n");

	/* WIZCHIP SOCKET Buffer 初始化 */
	if(ctlwizchip(CW_INIT_WIZCHIP,(void*)memsize) == -1){
		 printf("WIZCHIP Initialized fail.\r\n");
		 while(1);
	}

	/* PHY链路状态检查 */
	do{
		 if(ctlwizchip(CW_GET_PHYLINK, (void*)&tmp) == -1){
				printf("Unknown PHY Link stauts.\r\n");
		 }
	}while(tmp == PHY_LINK_OFF);

	/* 网络初始化 */
	network_init();
	
while(1)
	{
		delay_ms(50);
		switch(getSn_SR(SOCK_TCPS))														// 获取socket0的状态
		{
			case SOCK_INIT:															// Socket处于初始化完成(打开)状态	
					listen(SOCK_TCPS);
			break;
			case SOCK_ESTABLISHED:											// Socket处于连接建立状态
					if(getSn_IR(SOCK_TCPS) & Sn_IR_CON)   					
					{
						setSn_IR(SOCK_TCPS, Sn_IR_CON);								// Sn_IR的CON位置1，通知W5500连接已建立
					}
					// 数据回环测试程序：数据从上位机服务器发给W5500，W5500接收到数据后再回给服务器
					len=getSn_RX_RSR(SOCK_TCPS);										// len=Socket0接收缓存中已接收和保存的数据大小					
					if(len)
					{
						recv(SOCK_TCPS,gDATABUF,len);		
						printf("%s\r\n",gDATABUF);
						send(SOCK_TCPS,gDATABUF,len);							
					}											
			break;
			case SOCK_CLOSE_WAIT:												  // Socket处于等待关闭状态
				disconnect(SOCK_TCPS);	
			break;
			case SOCK_CLOSED:														// Socket处于关闭状态
					socket(SOCK_TCPS,Sn_MR_TCP,5000,0x00);		// 打开Socket0，打开一个本地端口
			break;
	  }
  }
}
/**
  * @brief  Intialize the network information to be used in WIZCHIP
  * @retval None
  */
void network_init(void)
{
  uint8_t tmpstr[6];
	ctlnetwork(CN_SET_NETINFO, (void*)&gWIZNETINFO);
	ctlnetwork(CN_GET_NETINFO, (void*)&gWIZNETINFO);

	// Display Network Information
	ctlwizchip(CW_GET_ID,(void*)tmpstr);
	printf("\r\n=== %s NET CONF ===\r\n",(char*)tmpstr);
	printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",gWIZNETINFO.mac[0],gWIZNETINFO.mac[1],gWIZNETINFO.mac[2],
		  gWIZNETINFO.mac[3],gWIZNETINFO.mac[4],gWIZNETINFO.mac[5]);
	printf("SIP: %d.%d.%d.%d\r\n", gWIZNETINFO.ip[0],gWIZNETINFO.ip[1],gWIZNETINFO.ip[2],gWIZNETINFO.ip[3]);
	printf("GAR: %d.%d.%d.%d\r\n", gWIZNETINFO.gw[0],gWIZNETINFO.gw[1],gWIZNETINFO.gw[2],gWIZNETINFO.gw[3]);
	printf("SUB: %d.%d.%d.%d\r\n", gWIZNETINFO.sn[0],gWIZNETINFO.sn[1],gWIZNETINFO.sn[2],gWIZNETINFO.sn[3]);
	printf("DNS: %d.%d.%d.%d\r\n", gWIZNETINFO.dns[0],gWIZNETINFO.dns[1],gWIZNETINFO.dns[2],gWIZNETINFO.dns[3]);
	printf("======================\r\n");
}

/**
  * @brief  Loopback Test Example Code using ioLibrary_BSD	
  * @retval None
  */
void platform_init(void)
{
  //SystemInit();//系统时钟初始化	
    Debug_USART_Config();//串口1初始化	
	//Config SPI
	SPI_Configuration();
	//延时初始化
	delay_init(168);
}



/*********************************************END OF FILE**********************/

