#ifndef _W5500_CONF_H_
#define _W5500_CONF_H_


#include "stm32f4xx.h"
#include "stdio.h"


#define WIZ_RESET             GPIO_Pin_7				  /* ����W5500��RESET�ܽ�         */
#define WIZ_SPIx_RESET_PORT   GPIOB						  /* GPIO�˿�                     */
#define WIZ_SPIx_RESET_CLK    RCC_AHB1Periph_GPIOB	 	  /* GPIO�˿�ʱ��                 */ 

#define WIZ_INT               GPIO_Pin_6				  /* ����W5500��INT�ܽ�           */
#define WIZ_SPIx_INT_PORT     GPIOB						  /* GPIO�˿�                     */
#define WIZ_SPIx_INT_CLK      RCC_AHB1Periph_GPIOB	      /* GPIO�˿�ʱ��                 */


void gpio_for_w5500_config(void);								/*SPI�ӿ�reset ���ж�����*/

void reset_w5500(void);											/*Ӳ��λW5500*/



#endif

