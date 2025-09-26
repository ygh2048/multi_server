/**
  ******************************************************************************
  * @file    main.c
  * @author  fire
  * @version V1.0
  * @date    2015-xx-xx
  * @brief   W5500ģ����DHCP����IP��ַ
  ******************************************************************************
  * @attention
  *
  * ʵ��ƽ̨:Ұ��  STM32 F407 ������  
  * ��̳    :http://www.firebbs.cn
  * �Ա�    :https://fire-stm32.taobao.com
  *
  ******************************************************************************
  */
 
#include <stdio.h>
#include <string.h> 
#include "stm32f4xx.h"
#include "./usart/bsp_debug_usart.h"
#include "./i2c/bsP_i2c_ee.h"
#include "./i2c/bsp_i2c_gpio.h"
#include "./led/bsp_led.h"

#include "w5500.h"
#include "W5500_conf.h"
#include "socket.h"
#include "utility.h"
/*app����ͷ�ļ�*/
#include "dhcp.h"

/**
  * @brief  ������
  * @param  ��
  * @retval ��
  */
int main(void)
{
    systick_init(180);	      /*��ʼ��Systick����ʱ��*/
    Debug_USART_Config();     /*��ʼ��USART1*/
    i2c_CfgGpio();            /*��ʼ��eeprom*/
    LED_GPIO_Config();        /*��ʼ��LED*/
	
	LED_BLUE;                  /*��ʼ״̬Ϊ����*/
    
    printf("  Ұ����������� �����ʼ�� Demo V1.0 \r\n");		

    reset_w5500();											/*Ӳ��λW5500*/
    gpio_for_w5500_config();						        /*��ʼ��MCU�������*/
    set_w5500_mac();										/*����MAC��ַ*/
    
    socket_buf_init(txsize, rxsize);		/*��ʼ��8��Socket�ķ��ͽ��ջ����С*/
	
    printf(" ��������ɳ�ʼ������\r\n");
    printf(" Ұ�������������ΪDHCP�ͻ��ˣ����Դ�DHCP��������ȡIP��ַ \r\n");

  
  while (1)                             /*ѭ��ִ�еĺ���*/ 
  {      
      do_dhcp();                        /*DHCP���Գ���*/
  }  

}



/*********************************************END OF FILE**********************/

