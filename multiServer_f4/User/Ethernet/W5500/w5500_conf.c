#include <stdio.h> 
#include <string.h>

#include "w5500_conf.h"


#include "delay.h"





/**
*@brief		配置W5500的GPIO接口
*@param		无
*@return	无
*/
void gpio_for_w5500_config(void)
{

  GPIO_InitTypeDef GPIO_InitStructure;

	
  /*定义RESET引脚*/
  GPIO_InitStructure.GPIO_Pin = WIZ_RESET;					    /*选择要控制的GPIO引脚*/		 
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		        /*设置引脚速率为50MHz */		
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;                 /*设置引脚模式为通用推挽输出*/
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP ;
  GPIO_Init(WIZ_SPIx_RESET_PORT, &GPIO_InitStructure);		    /*调用库函数，初始化GPIO*/
  GPIO_SetBits(WIZ_SPIx_RESET_PORT, WIZ_RESET);		
  /*定义INT引脚*/	
  GPIO_InitStructure.GPIO_Pin = WIZ_INT;						/*选择要控制的GPIO引脚*/		 
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		        /*设置引脚速率为50MHz*/		
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;                  /*设置引脚模式为通用上拉输入*/
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init(WIZ_SPIx_INT_PORT, &GPIO_InitStructure);		    /*调用库函数，初始化GPIO*/
  
}




/**
*@brief		W5500复位设置函数
*@param		无
*@return	无
*/
void reset_w5500(void)
{
	GPIO_ResetBits(WIZ_SPIx_RESET_PORT, WIZ_RESET);
	delay_ms(2);  
	GPIO_SetBits(WIZ_SPIx_RESET_PORT, WIZ_RESET);
	delay_ms(1600);
}



