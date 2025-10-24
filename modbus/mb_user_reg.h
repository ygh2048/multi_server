#ifndef _MB_USER_REG_H_
#define _MB_USER_REG_H_

#include "mb.h"

/* 保持寄存器起始地址和数量 */
#define REG_HOLDING_START    1      /* 起始地址为1（寄存器地址从1开始） */
#define REG_HOLDING_NREGS   16      /* 支持16个寄存器，0-15 */

/* 用户寄存器初始化 */
void vMBUserRegInit(void);

#endif /* _MB_USER_REG_H_ */