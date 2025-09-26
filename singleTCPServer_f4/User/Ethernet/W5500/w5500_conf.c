#include <stdio.h> 
#include <string.h>

#include "w5500_conf.h"


#include "delay.h"





/**
*@brief		����W5500��GPIO�ӿ�
*@param		��
*@return	��
*/
void gpio_for_w5500_config(void)
{

  GPIO_InitTypeDef GPIO_InitStructure;

	
  /*����RESET����*/
  GPIO_InitStructure.GPIO_Pin = WIZ_RESET;					    /*ѡ��Ҫ���Ƶ�GPIO����*/		 
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		        /*������������Ϊ50MHz */		
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;                 /*��������ģʽΪͨ���������*/
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP ;
  GPIO_Init(WIZ_SPIx_RESET_PORT, &GPIO_InitStructure);		    /*���ÿ⺯������ʼ��GPIO*/
  GPIO_SetBits(WIZ_SPIx_RESET_PORT, WIZ_RESET);		
  /*����INT����*/	
  GPIO_InitStructure.GPIO_Pin = WIZ_INT;						/*ѡ��Ҫ���Ƶ�GPIO����*/		 
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		        /*������������Ϊ50MHz*/		
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;                  /*��������ģʽΪͨ����������*/
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init(WIZ_SPIx_INT_PORT, &GPIO_InitStructure);		    /*���ÿ⺯������ʼ��GPIO*/
  
}




/**
*@brief		W5500��λ���ú���
*@param		��
*@return	��
*/
void reset_w5500(void)
{
	GPIO_ResetBits(WIZ_SPIx_RESET_PORT, WIZ_RESET);
	delay_ms(2);  
	GPIO_SetBits(WIZ_SPIx_RESET_PORT, WIZ_RESET);
	delay_ms(1600);
}



