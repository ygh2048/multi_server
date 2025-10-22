/******************** (C) COPYRIGHT 2013 **************************
 * �ļ���  ��port.h
 * ����    ��STM32 Modbus-TCP ���Գ���         
 * ʵ��ƽ̨��STM32F103VCT6+W5500 ������
 * ��汾  ��ST3.5.0
 *
 * ����    ��Zhangsz
 * ��д���ڣ�2013-12-06
**********************************************************************************/

#ifndef _PORT_H
#define _PORT_H

/* ----------------------- Platform includes --------------------------------*/

#include "sys.h"
#include <stm32f4xx_conf.h>

/* ----------------------- Defines ------------------------------------------*/
#define	INLINE
#define PR_BEGIN_EXTERN_C           extern "C" {
#define	PR_END_EXTERN_C             }



#define ENTER_CRITICAL_SECTION( )   EnterCriticalSection( )
#define EXIT_CRITICAL_SECTION( )    ExitCriticalSection( )
#define assert( expr )

//#define SMCLK                       ( 8000000UL )	 /*ϵͳ����8Mhz*/
//#define ACLK                        ( 32768UL )	  	 /*RTC���� 32768Hz */

//TODO  ��ʱ��дB13���ţ�����������ʱ��ȷ��
//#define RS485_SEND_MODE  GPIO_SetBits(GPIOA,GPIO_Pin_8)
//#define RS485_RECEIVE_MODE  GPIO_ResetBits(GPIOA,GPIO_Pin_8)


typedef char   			BOOL;		 //int8_t
typedef unsigned char	UCHAR;	  	 //uint8_t
typedef char    		CHAR;		 //int8_t
typedef unsigned short 	USHORT;		 //uint16_t
typedef short   		SHORT;		 //int16_t
typedef unsigned long 	ULONG;		 //uint32_t
typedef long    		LONG;		 //int32_t

#define TRUE	1
#define FALSE	0

void            EnterCriticalSection( void );
void            ExitCriticalSection( void );


#endif
