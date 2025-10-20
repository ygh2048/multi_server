#ifndef __TASK_TCP_H
#define __TASK_TCP_H

#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "includes.h"


//LED0任务
//设置任务优先级
#define TCP_TASK_PRIO       			7 
//设置任务堆栈大小
#define TCP_STK_SIZE  		    		64
//任务堆栈	
extern OS_STK TCP_TASK_STK[TCP_STK_SIZE];
//任务函数
void tcp_task(void *pdata);


//LED1任务
//设置任务优先级
#define LED1_TASK_PRIO       			6 
//设置任务堆栈大小
#define LED1_STK_SIZE  					64
//任务堆栈
extern OS_STK LED1_TASK_STK[LED1_STK_SIZE];
//任务函数
void led1_task(void *pdata);












#endif
