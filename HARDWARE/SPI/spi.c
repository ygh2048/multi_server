#include "spi.h"
#include "wiz_platform.h"
//////////////////////////////////////////////////////////////////////////////////	 
//±¾³ÌÐòÖ»¹©Ñ§Ï°Ê¹ÓÃ£¬Î´¾­×÷ÕßÐí¿É£¬²»µÃÓÃÓÚÆäËüÈÎºÎÓÃÍ¾
//ALIENTEK STM32F407¿ª·¢°å
//SPI Çý¶¯´úÂë	   
//ÕýµãÔ­×Ó@ALIENTEK
//¼¼ÊõÂÛÌ³:www.openedv.com
//´´½¨ÈÕÆÚ:2014/5/6
//°æ±¾£ºV1.0
//°æÈ¨ËùÓÐ£¬µÁ°æ±Ø¾¿¡£
//Copyright(C) ¹ãÖÝÊÐÐÇÒíµç×Ó¿Æ¼¼ÓÐÏÞ¹«Ë¾ 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	 


//ÒÔÏÂÊÇSPIÄ£¿éµÄ³õÊ¼»¯´úÂë£¬ÅäÖÃ³ÉÖ÷»úÄ£Ê½ 						  
//SPI¿Ú³õÊ¼»¯
//ÕâÀïÕëÊÇ¶ÔSPI1µÄ³õÊ¼»¯
void SPI1_Init(void)
{	 
  GPIO_InitTypeDef  GPIO_InitStructure;
  SPI_InitTypeDef  SPI_InitStructure;
	
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);//Ê¹ÄÜGPIOBÊ±ÖÓ
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);//Ê¹ÄÜSPI1Ê±ÖÓ
 
  //GPIOFB3,4,5³õÊ¼»¯ÉèÖÃ
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3|GPIO_Pin_4|GPIO_Pin_5;//PB3~5¸´ÓÃ¹¦ÄÜÊä³ö	
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//¸´ÓÃ¹¦ÄÜ
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//ÍÆÍìÊä³ö
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//ÉÏÀ­
  GPIO_Init(GPIOB, &GPIO_InitStructure);//³õÊ¼»¯
	
	GPIO_PinAFConfig(GPIOB,GPIO_PinSource3,GPIO_AF_SPI1); //PB3¸´ÓÃÎª SPI1
	GPIO_PinAFConfig(GPIOB,GPIO_PinSource4,GPIO_AF_SPI1); //PB4¸´ÓÃÎª SPI1
	GPIO_PinAFConfig(GPIOB,GPIO_PinSource5,GPIO_AF_SPI1); //PB5¸´ÓÃÎª SPI1
 
	//ÕâÀïÖ»Õë¶ÔSPI¿Ú³õÊ¼»¯
	RCC_APB2PeriphResetCmd(RCC_APB2Periph_SPI1,ENABLE);//¸´Î»SPI1
	RCC_APB2PeriphResetCmd(RCC_APB2Periph_SPI1,DISABLE);//Í£Ö¹¸´Î»SPI1

	SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;  //ÉèÖÃSPIµ¥Ïò»òÕßË«ÏòµÄÊý¾ÝÄ£Ê½:SPIÉèÖÃÎªË«ÏßË«ÏòÈ«Ë«¹¤
	SPI_InitStructure.SPI_Mode = SPI_Mode_Master;		//ÉèÖÃSPI¹¤×÷Ä£Ê½:ÉèÖÃÎªÖ÷SPI
	SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;		//ÉèÖÃSPIµÄÊý¾Ý´óÐ¡:SPI·¢ËÍ½ÓÊÕ8Î»Ö¡½á¹¹
	SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;		//´®ÐÐÍ¬²½Ê±ÖÓµÄ¿ÕÏÐ×´Ì¬Îª¸ßµçÆ½
	SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;	//´®ÐÐÍ¬²½Ê±ÖÓµÄµÚ¶þ¸öÌø±äÑØ£¨ÉÏÉý»òÏÂ½µ£©Êý¾Ý±»²ÉÑù
	SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;		//NSSÐÅºÅÓÉÓ²¼þ£¨NSS¹Ü½Å£©»¹ÊÇÈí¼þ£¨Ê¹ÓÃSSIÎ»£©¹ÜÀí:ÄÚ²¿NSSÐÅºÅÓÐSSIÎ»¿ØÖÆ
	SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;		//¶¨Òå²¨ÌØÂÊÔ¤·ÖÆµµÄÖµ:²¨ÌØÂÊÔ¤·ÖÆµÖµÎª256
	SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;	//Ö¸¶¨Êý¾Ý´«Êä´ÓMSBÎ»»¹ÊÇLSBÎ»¿ªÊ¼:Êý¾Ý´«Êä´ÓMSBÎ»¿ªÊ¼
	SPI_InitStructure.SPI_CRCPolynomial = 7;	//CRCÖµ¼ÆËãµÄ¶àÏîÊ½
	SPI_Init(SPI1, &SPI_InitStructure);  //¸ù¾ÝSPI_InitStructÖÐÖ¸¶¨µÄ²ÎÊý³õÊ¼»¯ÍâÉèSPIx¼Ä´æÆ÷
 
	SPI_Cmd(SPI1, ENABLE); //Ê¹ÄÜSPIÍâÉè

	SPI1_ReadWriteByte(0xff);//Æô¶¯´«Êä		 
}   
//SPI1ËÙ¶ÈÉèÖÃº¯Êý
//SPIËÙ¶È=fAPB2/·ÖÆµÏµÊý
//@ref SPI_BaudRate_Prescaler:SPI_BaudRatePrescaler_2~SPI_BaudRatePrescaler_256  
//fAPB2Ê±ÖÓÒ»°ãÎª84Mhz£º
void SPI1_SetSpeed(u8 SPI_BaudRatePrescaler)
{
  assert_param(IS_SPI_BAUDRATE_PRESCALER(SPI_BaudRatePrescaler));//ÅÐ¶ÏÓÐÐ§ÐÔ
	SPI1->CR1&=0XFFC7;//Î»3-5ÇåÁã£¬ÓÃÀ´ÉèÖÃ²¨ÌØÂÊ
	SPI1->CR1|=SPI_BaudRatePrescaler;	//ÉèÖÃSPI1ËÙ¶È 
	SPI_Cmd(SPI1,ENABLE); //Ê¹ÄÜSPI1
} 
//SPI1 ¶ÁÐ´Ò»¸ö×Ö½Ú
//TxData:ÒªÐ´ÈëµÄ×Ö½Ú
//·µ»ØÖµ:¶ÁÈ¡µ½µÄ×Ö½Ú
u8 SPI1_ReadWriteByte(u8 TxData)
{		 			 
 
  while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET){}//µÈ´ý·¢ËÍÇø¿Õ  
	
	SPI_I2S_SendData(SPI1, TxData); //Í¨¹ýÍâÉèSPIx·¢ËÍÒ»¸öbyte  Êý¾Ý
		
  while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET){} //µÈ´ý½ÓÊÕÍêÒ»¸öbyte  
 
	return SPI_I2S_ReceiveData(SPI1); //·µ»ØÍ¨¹ýSPIx×î½ü½ÓÊÕµÄÊý¾Ý	
 		    
}


#include "stm32f4xx.h"

//==================================================
// SPI2_Init : PB13(SCK) PB14(MISO) PB15(MOSI)
//==================================================
void SPI2_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    SPI_InitTypeDef   SPI_InitStructure;

    // 1) ????:GPIOB(AHB1) & SPI2(APB1)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2,  ENABLE);

    // 2) GPIO ????:PB13/14/15 -> SPI2
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_PinAFConfig(GPIOB, GPIO_PinSource13, GPIO_AF_SPI2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource14, GPIO_AF_SPI2);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource15, GPIO_AF_SPI2);

    // 3) ???(??)
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_SPI2, DISABLE);

    // 4) SPI ??:CPOL=High, CPHA=2Edge, 8bit, ??, ?NSS, MSB, ????
    SPI_I2S_DeInit(SPI2);
    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
		SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;   // Mode 0
		SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge; // Mode 0

    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256; // ˜42MHz/256 ˜ 164 kHz
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI2, &SPI_InitStructure);

    // 5) ?? SPI2,???????
    SPI_Cmd(SPI2, ENABLE);
    SPI_I2S_ReceiveData(SPI2);           // ???DR
    SPI2_ReadWriteByte(0xFF);            // ??????
}

//==================================================
// SPI2_SetSpeed : ???????
// ??:SPI_BaudRatePrescaler_2/_4/_8/_16/_32/_64/_128/_256
//==================================================
void SPI2_SetSpeed(u16 SPI_BaudRatePrescaler)
{
    assert_param(IS_SPI_BAUDRATE_PRESCALER(SPI_BaudRatePrescaler));
    SPI_Cmd(SPI2, DISABLE);
    SPI2->CR1 &= ~(SPI_CR1_BR);                 // ????
    SPI2->CR1 |=  SPI_BaudRatePrescaler;        // ????
    SPI_Cmd(SPI2, ENABLE);
}

//==================================================
// SPI2_ReadWriteByte : ??????
//==================================================
u8 SPI2_ReadWriteByte(u8 TxData)
{
    // ?TXE??
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET) {}
    SPI_I2S_SendData(SPI2, TxData);

    // ??????
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_RXNE) == RESET) {}

    return (u8)SPI_I2S_ReceiveData(SPI2);
}










