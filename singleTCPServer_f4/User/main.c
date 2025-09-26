/**
  ******************************************************************************
  * @file    main.c
  * @author  WIZnet Software Team
  * @version V1.0
  * @date    2015-02-14
  * @brief   W5500ģ����TCPServer
  ******************************************************************************
  * @attention
  *
  * ʵ��ƽ̨:Ұ��  STM32 F407 ������  
  * ��̳    :http://www.firebbs.cn
  * �Ա�    :https://fire-stm32.taobao.com
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
// Ĭ����������
wiz_NetInfo gWIZNETINFO = { .mac = {0x00, 0x08, 0xdc,0x11, 0x11, 0x11},
                            .ip = {192, 168, 103, 202},
                            .sn = {255,255,255,0},
                            .gw = {192, 168, 103, 1},
                            .dns = {8,8,8,8},
                            .dhcp = NETINFO_STATIC };
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
void platform_init(void);								// ��ʼ����ص���������
void network_init(void);								// ��ʼ��������Ϣ����ʾ

/****************************************************
��������		main
�βΣ�		��
����ֵ��		��
�������ܣ�	������
****************************************************/
int main(void)
{
	uint8_t tmp;

	uint16_t len=0;
	uint8_t memsize[2][8] = {{2,2,2,2,2,2,2,2},{2,2,2,2,2,2,2,2}};
//	uint8_t DstIP[4]={192,168,103,19};  //Ŀ��IP��ַ
//	uint16_t	DstPort=6000;           //�˿ں�
	//��ʼ����������ص�����
	platform_init();
	// ���ȣ�Ӧ��ע���û�Ϊ����WIZCHIPʵ�ֵ�SPI�ص����� 
	/* �ٽ����ص� */
	reg_wizchip_cris_cbfunc(SPI_CrisEnter, SPI_CrisExit);	//ע���ٽ�������
	/* Ƭѡ�ص� */
#if   _WIZCHIP_IO_MODE_ == _WIZCHIP_IO_MODE_SPI_VDM_
	reg_wizchip_cs_cbfunc(SPI_CS_Select, SPI_CS_Deselect);  //ע��SPIƬѡ�źź���
#elif _WIZCHIP_IO_MODE_ == _WIZCHIP_IO_MODE_SPI_FDM_
	reg_wizchip_cs_cbfunc(SPI_CS_Select, SPI_CS_Deselect);  // CS must be tried with LOW.
#else
   #if (_WIZCHIP_IO_MODE_ & _WIZCHIP_IO_MODE_SIP_) != _WIZCHIP_IO_MODE_SIP_
      #error "Unknown _WIZCHIP_IO_MODE_"
   #else
      reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
   #endif
#endif
	/* SPI����д�ص����� */
	reg_wizchip_spi_cbfunc(SPI_ReadByte, SPI_WriteByte);	//ע���д����
    printf("��ʼ�� Demo V1.0 \r\n");	
    gpio_for_w5500_config();						        /*��ʼ��MCU�������*/
    reset_w5500();											/*Ӳ��λW5500*/

	 printf("reset_w5500() over \r\n");

	/* WIZCHIP SOCKET Buffer ��ʼ�� */
	if(ctlwizchip(CW_INIT_WIZCHIP,(void*)memsize) == -1){
		 printf("WIZCHIP Initialized fail.\r\n");
		 while(1);
	}

	/* PHY��·״̬��� */
	do{
		 if(ctlwizchip(CW_GET_PHYLINK, (void*)&tmp) == -1){
				printf("Unknown PHY Link stauts.\r\n");
		 }
	}while(tmp == PHY_LINK_OFF);

	/* �����ʼ�� */
	network_init();
	
while(1)
	{
		delay_ms(50);
		switch(getSn_SR(SOCK_TCPS))														// ��ȡsocket0��״̬
		{
			case SOCK_INIT:															// Socket���ڳ�ʼ�����(��)״̬	
					listen(SOCK_TCPS);
			break;
			case SOCK_ESTABLISHED:											// Socket�������ӽ���״̬
					if(getSn_IR(SOCK_TCPS) & Sn_IR_CON)   					
					{
						setSn_IR(SOCK_TCPS, Sn_IR_CON);								// Sn_IR��CONλ��1��֪ͨW5500�����ѽ���
					}
					// ���ݻػ����Գ������ݴ���λ������������W5500��W5500���յ����ݺ��ٻظ�������
					len=getSn_RX_RSR(SOCK_TCPS);										// len=Socket0���ջ������ѽ��պͱ�������ݴ�С					
					if(len)
					{
						recv(SOCK_TCPS,gDATABUF,len);		
						printf("%s\r\n",gDATABUF);
						send(SOCK_TCPS,gDATABUF,len);							
					}											
			break;
			case SOCK_CLOSE_WAIT:												  // Socket���ڵȴ��ر�״̬
				disconnect(SOCK_TCPS);	
			break;
			case SOCK_CLOSED:														// Socket���ڹر�״̬
					socket(SOCK_TCPS,Sn_MR_TCP,5000,0x00);		// ��Socket0����һ�����ض˿�
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
  //SystemInit();//ϵͳʱ�ӳ�ʼ��	
    Debug_USART_Config();//����1��ʼ��	
	//Config SPI
	SPI_Configuration();
	//��ʱ��ʼ��
	delay_init(168);
}



/*********************************************END OF FILE**********************/

