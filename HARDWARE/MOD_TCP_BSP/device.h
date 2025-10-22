#ifndef __DEVICE_H
#define	__DEVICE_H



#define S_RX_SIZE	2048
#define S_TX_SIZE	2048


typedef  unsigned char SOCKET;

#include "wiz_platform.h"  //硬件配置

#define W5500_SCS_LOW				    GPIO_ResetBits(WIZ_SCS_PORT, WIZ_SCS_PIN)
#define W5500_SCS_HIGH					GPIO_SetBits(WIZ_SCS_PORT, WIZ_SCS_PIN)												

#include "port.h"

#include "includes.h"
#include "socket.h"	
#include "usart.h"	
#include "sys.h"
#include "delay.h"
#include "timer.h"

#include "w5500.h"













#endif

