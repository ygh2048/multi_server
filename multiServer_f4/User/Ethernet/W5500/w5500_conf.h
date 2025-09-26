#ifndef _W5500_CONF_H_
#define _W5500_CONF_H_


#include "stm32f4xx.h"
#include "stdio.h"


#define WIZ_RESET             GPIO_Pin_7				  /* 定义W5500的RESET管脚         */
#define WIZ_SPIx_RESET_PORT   GPIOB						  /* GPIO端口                     */
#define WIZ_SPIx_RESET_CLK    RCC_AHB1Periph_GPIOB	 	  /* GPIO端口时钟                 */ 

#define WIZ_INT               GPIO_Pin_6				  /* 定义W5500的INT管脚           */
#define WIZ_SPIx_INT_PORT     GPIOB						  /* GPIO端口                     */
#define WIZ_SPIx_INT_CLK      RCC_AHB1Periph_GPIOB	      /* GPIO端口时钟                 */


void gpio_for_w5500_config(void);								/*SPI接口reset 及中断引脚*/

void reset_w5500(void);											/*硬复位W5500*/



#endif

