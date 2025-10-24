#include "mb_user_reg.h"
#include "mb.h"
#include "mbport.h"
#include "led.h"

/* 用户保持寄存器（用于LED控制） */
static USHORT usRegHoldingBuf[REG_HOLDING_NREGS] = {0};

/* 用于控制LED的寄存器地址（偏移量） */
#define REG_LED1_ADDR    0  /* LED1 控制位地址 */
#define REG_LED2_ADDR    1  /* LED2 控制位地址 */

/* 初始化用户寄存器 */
void vMBUserRegInit(void)
{
    /* 将LED寄存器初始化为0（LED关闭） */
    usRegHoldingBuf[REG_LED1_ADDR] = 0;
    usRegHoldingBuf[REG_LED2_ADDR] = 0;
    LED0 = 1;  /* LED1 关闭（高电平） */
    LED1 = 1;  /* LED2 关闭（高电平） */
}

/* 保持寄存器回调函数 */
eMBErrorCode eMBRegHoldingCB(UCHAR * pucRegBuffer, USHORT usAddress,
                            USHORT usNRegs, eMBRegisterMode eMode)
{
    eMBErrorCode eStatus = MB_ENOERR;
    int iRegIndex;

    /* 检查地址是否在范围内 */
    if((usAddress >= REG_HOLDING_START) &&
       (usAddress + usNRegs <= REG_HOLDING_START + REG_HOLDING_NREGS))
    {
        iRegIndex = (int)(usAddress - REG_HOLDING_START);
        
        switch(eMode)
        {
            case MB_REG_READ:
                /* 从保持寄存器读取数据 */
                while(usNRegs > 0)
                {
                    *pucRegBuffer++ = (UCHAR)(usRegHoldingBuf[iRegIndex] >> 8);
                    *pucRegBuffer++ = (UCHAR)(usRegHoldingBuf[iRegIndex] & 0xFF);
                    iRegIndex++;
                    usNRegs--;
                }
                break;

            case MB_REG_WRITE:
                /* 写入数据到保持寄存器 */
                while(usNRegs > 0)
                {
                    USHORT value;
                    value = (USHORT)(*pucRegBuffer++ << 8);
                    value |= (USHORT)(*pucRegBuffer++);
                    usRegHoldingBuf[iRegIndex] = value;

                    /* 根据寄存器值控制LED */
                    if(iRegIndex == REG_LED1_ADDR)
                    {
                        LED0 = (value == 0) ? 1 : 0;  /* LED1（低电平有效） */
                    }
                    else if(iRegIndex == REG_LED2_ADDR)
                    {
                        LED1 = (value == 0) ? 1 : 0;  /* LED2（低电平有效） */
                    }

                    iRegIndex++;
                    usNRegs--;
                }
                break;
        }
    }
    else
    {
        eStatus = MB_ENOREG;
    }

    return eStatus;
}

/* 输入寄存器回调函数（如果需要） */
eMBErrorCode eMBRegInputCB(UCHAR * pucRegBuffer, USHORT usAddress,
                          USHORT usNRegs)
{
    return MB_ENOREG; /* 暂不支持输入寄存器 */
}

/* 线圈回调函数（如果需要） */
eMBErrorCode eMBRegCoilsCB(UCHAR * pucRegBuffer, USHORT usAddress,
                          USHORT usNCoils, eMBRegisterMode eMode)
{
    return MB_ENOREG; /* 暂不支持线圈 */
}

/* 离散输入回调函数（如果需要） */
eMBErrorCode eMBRegDiscreteCB(UCHAR * pucRegBuffer, USHORT usAddress,
                             USHORT usNDiscrete)
{
    return MB_ENOREG; /* 暂不支持离散输入 */
}