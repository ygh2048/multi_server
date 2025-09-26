/**
  ******************************************************************************
  * @file    main.c
  * @author  fire
  * @version V1.0
  * @date    2015-xx-xx
  * @brief   W5500模块下DHCP分配IP地址
  ******************************************************************************
  * @attention
  *
  * 实验平台:野火  STM32 F407 开发板  
  * 论坛    :http://www.firebbs.cn
  * 淘宝    :https://fire-stm32.taobao.com
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
/*app函数头文件*/
#include "dhcp.h"

/**
  * @brief  主函数
  * @param  无
  * @retval 无
  */
int main(void)
{
    systick_init(180);	      /*初始化Systick工作时钟*/
    Debug_USART_Config();     /*初始化USART1*/
    i2c_CfgGpio();            /*初始化eeprom*/
    LED_GPIO_Config();        /*初始化LED*/
	
	LED_BLUE;                  /*初始状态为蓝灯*/
    
    printf("  野火网络适配版 网络初始化 Demo V1.0 \r\n");		

    reset_w5500();											/*硬复位W5500*/
    gpio_for_w5500_config();						        /*初始化MCU相关引脚*/
    set_w5500_mac();										/*配置MAC地址*/
    
    socket_buf_init(txsize, rxsize);		/*初始化8个Socket的发送接收缓存大小*/
	
    printf(" 网络已完成初始化……\r\n");
    printf(" 野火网络适配板作为DHCP客户端，尝试从DHCP服务器获取IP地址 \r\n");

  
  while (1)                             /*循环执行的函数*/ 
  {      
      do_dhcp();                        /*DHCP测试程序*/
  }  

}



/*********************************************END OF FILE**********************/

