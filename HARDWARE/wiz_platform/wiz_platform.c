#include <stdio.h>
#include <stdint.h>
#include "wiz_platform.h"
#include "wizchip_conf.h"
#include "wiz_interface.h"
#include <stm32f4xx.h>
#include "dhcp.h"
#include "includes.h"	
#include "usart.h"	
#include "sys.h"
#include "delay.h"
#include "timer.h"
#include "spi.h"

void debug_usart_init(void)
{
		uart_init(115200);
    /*
	

    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);



    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);
		
    USART_Cmd(USART1, ENABLE);
		
	  */
}

/*
int fputc(int ch, FILE *f)
{
    USART1->SR;
    USART_SendData(USART1, (uint8_t) ch);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
    return (ch);
}

int fgetc(FILE *f)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);
    return (int)USART_ReceiveData(USART1);
}


*/

/*
 * TIM_Period / Auto Reload Register(ARR) = 1000   TIM_Prescaler--71 
 * interrupt period = 1/(72MHZ /72) * 1000 = 1ms
 *
 * TIMxCLK/CK_PSC --> TIMxCNT --> TIM_Period(ARR) --> TIMxCNT reset to 0 to restart counting
 */
void wiz_timer_init(void)
{
	/*
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_TimeBaseStructure.TIM_Period = 1000 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_0);
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    TIM_Cmd(TIM2, ENABLE);
		*/
		
		TIM3_Int_Init(1000-1,84-1);
		
}



void wiz_spi_init(void)
{
	/*
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOD, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
	
    SPI_InitTypeDef SPI_InitStructure;
    SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_Init(SPI2, &SPI_InitStructure);
    SPI_Cmd(SPI2, ENABLE);
	
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = WIZ_SCS_PIN;
    GPIO_Init(WIZ_SCS_PORT, &GPIO_InitStructure);
    GPIO_SetBits(WIZ_SCS_PORT, WIZ_SCS_PIN);
		
		*/
		
		GPIO_InitTypeDef GPIO_InitStructure;
		
		SPI2_Init();
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    GPIO_InitStructure.GPIO_Pin   = WIZ_SCS_PIN;          // PD7
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(WIZ_SCS_PORT, &GPIO_InitStructure);
    GPIO_SetBits(WIZ_SCS_PORT, WIZ_SCS_PIN);              // CS

    SPI2_SetSpeed(SPI_BaudRatePrescaler_32);          

    SPI2_ReadWriteByte(0xFF);
}

void wiz_rst_int_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // F4 ? GPIOD ? AHB1 
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    // PD_08 -> RST
    GPIO_InitStructure.GPIO_Pin   = WIZ_RST_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(WIZ_RST_PORT, &GPIO_InitStructure);
    GPIO_SetBits(WIZ_RST_PORT, WIZ_RST_PIN);

    // PD_09 -> INT ????
    GPIO_InitStructure.GPIO_Pin   = WIZ_INT_PIN;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(WIZ_INT_PORT, &GPIO_InitStructure);
}
/*


void delay_init(void)
{

	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
	UsNumber = SystemCoreClock / 8000000;
	MsNumber = (u16)UsNumber * 1000;
}


void delay_us(uint32_t nus)
{
	uint32_t temp;
	SysTick->LOAD = nus * UsNumber;
	SysTick->VAL = 0x00;
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
	do
	{
		temp = SysTick->CTRL;
	} while ((temp & 0x01) && !(temp & (1 << 16)));
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
	SysTick->VAL = 0X00;
}


void delay_ms(uint32_t nms)
{
	uint32_t temp;
	SysTick->LOAD = (uint32_t)nms * MsNumber;
	SysTick->VAL = 0x00;
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
	do
	{
		temp = SysTick->CTRL;
	} while ((temp & 0x01) && !(temp & (1 << 16)));
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
	SysTick->VAL = 0X00;
}


*/

/**
 * @brief   delay s
 * @param   none
 * @return  none
 */
void delay_s(uint32_t ns)
{
		while(ns--)
		{
				delay_ms(1000);
		}
}



/**
 * @brief   SPI select wizchip
 * @param   none
 * @return  none
 */
void wizchip_select(void)
{
    GPIO_ResetBits(WIZ_SCS_PORT, WIZ_SCS_PIN);
}

/**
 * @brief   SPI deselect wizchip
 * @param   none
 * @return  none
 */
void wizchip_deselect(void)
{
    GPIO_SetBits(WIZ_SCS_PORT, WIZ_SCS_PIN);
}

void wizchip_write_byte(uint8_t dat) { (void)SPI2_ReadWriteByte(dat); }
uint8_t wizchip_read_byte(void)      { return SPI2_ReadWriteByte(0xFF); }



/**
 * @brief   SPI write buff from wizchip
 * @param   buff:write buff
 * @param   len:write len
 * @return  none
 */
void wizchip_write_buff(uint8_t *buf, uint16_t len)
{
    uint16_t idx = 0;
    for (idx = 0; idx < len; idx++)
    {
        wizchip_write_byte(buf[idx]);
    }
}

/**
 * @brief   SPI read buff from wizchip
 * @param   buff:read buff
 * @param   len:read len
 * @return  none
 */
void wizchip_read_buff(uint8_t *buf, uint16_t len)
{
    uint16_t idx = 0;
    for (idx = 0; idx < len; idx++)
    {
        buf[idx] = wizchip_read_byte();
    }
}

/**
 * @brief   hardware reset wizchip
 * @param   none
 * @return  none
 */
void wizchip_reset(void)
{
    GPIO_SetBits(WIZ_RST_PORT,WIZ_RST_PIN);
    delay_ms(10);
    GPIO_ResetBits(WIZ_RST_PORT,WIZ_RST_PIN);
    delay_ms(10);
    GPIO_SetBits(WIZ_RST_PORT,WIZ_RST_PIN);
    delay_ms(10);
}

/**
 * @brief   wizchip spi callback register
 * @param   none
 * @return  none
 */
void wizchip_spi_cb_reg(void)
{
    reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
    reg_wizchip_spi_cbfunc(wizchip_read_byte, wizchip_write_byte);
    reg_wizchip_spiburst_cbfunc(wizchip_read_buff, wizchip_write_buff);
}

/**
 * @brief   Hardware Platform Timer Interrupt Callback Function
 */
void TIM3_IRQHandler(void)
{
    static uint32_t wiz_timer_1ms_count = 0;

    if (TIM_GetITStatus(TIM3, TIM_IT_Update) == SET)
    {
        wiz_timer_1ms_count++;
        if (wiz_timer_1ms_count >= 1000)
        {
            DHCP_time_handler();
            wiz_timer_1ms_count = 0;
        }
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    }
}
