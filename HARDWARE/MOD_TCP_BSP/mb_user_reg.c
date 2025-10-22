// mb_user_reg.c  (C89 ????)
#include "mb.h"
#include "mbport.h"
#include "mb_user_reg.h"
#include <string.h>
#include "led.h"

/* ???????????? */
#define REG_HOLDING_START   1
#define REG_HOLDING_N       100

#define REG_INPUT_START     1
#define REG_INPUT_N         100

#define REG_COILS_START     1
#define REG_COILS_N         64

#define REG_DISC_START      1
#define REG_DISC_N          64



#define LED0_COIL_ADDR 1  // 地址 1 控制 LED0
#define LED1_COIL_ADDR 2  // 地址 2 控制 LED1





static USHORT usRegHoldingBuf[REG_HOLDING_N];
static USHORT usRegInputBuf[REG_INPUT_N];
static UCHAR  ucCoilsBuf[(REG_COILS_N + 7) / 8];
static UCHAR  ucDiscBuf[(REG_DISC_N  + 7) / 8];


/* Holding Registers: R/W */
eMBErrorCode eMBRegHoldingCB( UCHAR * pucRegBuffer, USHORT usAddress,
                              USHORT usNRegs, eMBRegisterMode eMode )
{
    USHORT i;

    if ( (usAddress < REG_HOLDING_START) ||
         (usAddress + usNRegs - 1 > REG_HOLDING_START + REG_HOLDING_N - 1) )
    {
        return MB_ENOREG;
    }

    i = (USHORT)(usAddress - REG_HOLDING_START);
    if (eMode == MB_REG_READ)
    {
        while (usNRegs--)
        {
            *pucRegBuffer++ = (UCHAR)(usRegHoldingBuf[i] >> 8);
            *pucRegBuffer++ = (UCHAR)(usRegHoldingBuf[i] & 0xFF);
            i++;
        }
    }
    else /* MB_REG_WRITE */
    {
        while (usNRegs--)
        {
            usRegHoldingBuf[i]  = (USHORT)(*pucRegBuffer++ << 8);
            usRegHoldingBuf[i] |= (USHORT)(*pucRegBuffer++     );
            i++;
        }
    }
    return MB_ENOERR;
}


/* Input Registers: R */
eMBErrorCode eMBRegInputCB( UCHAR * pucRegBuffer, USHORT usAddress, USHORT usNRegs )
{
    USHORT i;

    if ( (usAddress < REG_INPUT_START) ||
         (usAddress + usNRegs - 1 > REG_INPUT_START + REG_INPUT_N - 1) )
    {
        return MB_ENOREG;
    }

    i = (USHORT)(usAddress - REG_INPUT_START);
    while (usNRegs--)
    {
        *pucRegBuffer++ = (UCHAR)(usRegInputBuf[i] >> 8);
        *pucRegBuffer++ = (UCHAR)(usRegInputBuf[i] & 0xFF);
        i++;
    }
    return MB_ENOERR;
}



eMBErrorCode eMBRegCoilsCB( UCHAR * pucDest, USHORT usAddress,
                            USHORT usNCoils, eMBRegisterMode eMode )
{
    USHORT idx;
    UCHAR bit, mask;

    /* 如果地址不在可用范围内，返回错误 */
    if (usAddress < LED0_COIL_ADDR || usAddress + usNCoils - 1 > LED1_COIL_ADDR)
    {
        return MB_ENOREG;
    }

    idx = usAddress - LED0_COIL_ADDR;  // 计算线圈在内存中的索引

    /* 处理读请求（MB_REG_READ） */
    if (eMode == MB_REG_READ)
    {
        *pucDest = (ucCoilsBuf[idx / 8] & (1 << (idx % 8))) ? 1 : 0;
    }
    /* 处理写请求（MB_REG_WRITE） */
    else if (eMode == MB_REG_WRITE)
    {
        mask = (UCHAR)(1 << (idx % 8));
        if (*pucDest == 1)
        {
            ucCoilsBuf[idx / 8] |= mask;  // 设置为 1（点亮 LED）
        }
        else
        {
            ucCoilsBuf[idx / 8] &= ~mask;  // 设置为 0（熄灭 LED）
        }

        /* 根据地址控制 LED */
        if (idx == 0) {  // LED0
            if (*pucDest) {
                LED0 = 0;  // 点亮 LED0
            } else {
                LED0 = 1;  // 熄灭 LED0
            }
        }
        else if (idx == 1) {  // LED1
            if (*pucDest) {
                LED1 = 0;  // 点亮 LED1
            } else {
                LED1 = 1;  // 熄灭 LED1
            }
        }
    }
    return MB_ENOERR;
}


/* Discrete Inputs: bit R */
eMBErrorCode eMBRegDiscreteCB( UCHAR * pucDest, USHORT usAddress, USHORT usNDiscrete )
{
    USHORT idx;
    USHORT n;
    UCHAR  outMask;
    USHORT byte;
    UCHAR  bit;
    UCHAR  mask;

    if ( (usAddress < REG_DISC_START) ||
         (usAddress + usNDiscrete - 1 > REG_DISC_START + REG_DISC_N - 1) )
    {
        return MB_ENOREG;
    }

    idx = (USHORT)(usAddress - REG_DISC_START);

    outMask = 0x01;
    *pucDest = 0;

    for (n = 0; n < usNDiscrete; n++, idx++)
    {
        byte = (USHORT)(idx / 8);
        bit  = (UCHAR)(idx % 8);
        mask = (UCHAR)(1u << bit);

        if (ucDiscBuf[byte] & mask)
        {
            *pucDest |= outMask;
        }

        outMask <<= 1;
        if (outMask == 0)
        {
            pucDest++;
            *pucDest = 0;
            outMask = 0x01;
        }
    }
    return MB_ENOERR;
}




/* ??:?????? */
void MBUSR_Init(void)
{
    /* ??:???????????? */
    /* memset(usRegHoldingBuf, 0, sizeof(usRegHoldingBuf)); */
    /* memset(usRegInputBuf,   0, sizeof(usRegInputBuf));   */
    /* memset(ucCoilsBuf,      0, sizeof(ucCoilsBuf));      */
    /* memset(ucDiscBuf,       0, sizeof(ucDiscBuf));       */
}

/* ---------- Holding Registers ---------- */
int MBUSR_SetHoldingIdx(unsigned short idx0, unsigned short val)
{
    if (idx0 >= REG_HOLDING_N) return MBUSR_ERR_RANGE;
    EnterCriticalSection();
    usRegHoldingBuf[idx0] = val;
    ExitCriticalSection();
    return MBUSR_OK;
}

int MBUSR_GetHoldingIdx(unsigned short idx0, unsigned short *pVal)
{
    if (pVal == NULL) return MBUSR_ERR_ARG;
    if (idx0 >= REG_HOLDING_N) return MBUSR_ERR_RANGE;
    EnterCriticalSection();
    *pVal = usRegHoldingBuf[idx0];
    ExitCriticalSection();
    return MBUSR_OK;
}

int MBUSR_SetHoldingByAddr(unsigned short addr1b, unsigned short val)
{
    if (addr1b < REG_HOLDING_START) return MBUSR_ERR_RANGE;
    return MBUSR_SetHoldingIdx((unsigned short)(addr1b - REG_HOLDING_START), val);
}

int MBUSR_GetHoldingByAddr(unsigned short addr1b, unsigned short *pVal)
{
    if (addr1b < REG_HOLDING_START) return MBUSR_ERR_RANGE;
    return MBUSR_GetHoldingIdx((unsigned short)(addr1b - REG_HOLDING_START), pVal);
}

/* ---------- Input Registers ---------- */
int MBUSR_SetInputIdx(unsigned short idx0, unsigned short val)
{
    if (idx0 >= REG_INPUT_N) return MBUSR_ERR_RANGE;
    EnterCriticalSection();
    usRegInputBuf[idx0] = val;
    ExitCriticalSection();
    return MBUSR_OK;
}

int MBUSR_GetInputIdx(unsigned short idx0, unsigned short *pVal)
{
    if (pVal == NULL) return MBUSR_ERR_ARG;
    if (idx0 >= REG_INPUT_N) return MBUSR_ERR_RANGE;
    EnterCriticalSection();
    *pVal = usRegInputBuf[idx0];
    ExitCriticalSection();
    return MBUSR_OK;
}

int MBUSR_SetInputByAddr(unsigned short addr1b, unsigned short val)
{
    if (addr1b < REG_INPUT_START) return MBUSR_ERR_RANGE;
    return MBUSR_SetInputIdx((unsigned short)(addr1b - REG_INPUT_START), val);
}

int MBUSR_GetInputByAddr(unsigned short addr1b, unsigned short *pVal)
{
    if (addr1b < REG_INPUT_START) return MBUSR_ERR_RANGE;
    return MBUSR_GetInputIdx((unsigned short)(addr1b - REG_INPUT_START), pVal);
}

/* ---------- Coils (bitmap) ---------- */
int MBUSR_SetCoilIdx(unsigned short idx0, int on)
{
    unsigned short byte;
    unsigned char  bit;
    unsigned char  mask;
    if (idx0 >= REG_COILS_N) return MBUSR_ERR_RANGE;
    byte = (unsigned short)(idx0 / 8);
    bit  = (unsigned char)(idx0 % 8);
    mask = (unsigned char)(1u << bit);
    EnterCriticalSection();
    if (on) ucCoilsBuf[byte] |= mask;
    else    ucCoilsBuf[byte] &= (unsigned char)(~mask);
    ExitCriticalSection();
    return MBUSR_OK;
}

int MBUSR_GetCoilIdx(unsigned short idx0, int *pOn)
{
    unsigned short byte;
    unsigned char  bit;
    unsigned char  mask;
    if (pOn == NULL) return MBUSR_ERR_ARG;
    if (idx0 >= REG_COILS_N) return MBUSR_ERR_RANGE;
    byte = (unsigned short)(idx0 / 8);
    bit  = (unsigned char)(idx0 % 8);
    mask = (unsigned char)(1u << bit);
    EnterCriticalSection();
    *pOn = (ucCoilsBuf[byte] & mask) ? 1 : 0;
    ExitCriticalSection();
    return MBUSR_OK;
}

int MBUSR_SetCoilByAddr(unsigned short addr1b, int on)
{
    if (addr1b < REG_COILS_START) return MBUSR_ERR_RANGE;
    return MBUSR_SetCoilIdx((unsigned short)(addr1b - REG_COILS_START), on);
}

int MBUSR_GetCoilByAddr(unsigned short addr1b, int *pOn)
{
    if (addr1b < REG_COILS_START) return MBUSR_ERR_RANGE;
    return MBUSR_GetCoilIdx((unsigned short)(addr1b - REG_COILS_START), pOn);
}

/* ---------- Discrete Inputs (bitmap) ---------- */
int MBUSR_SetDiscreteIdx(unsigned short idx0, int on)
{
    unsigned short byte;
    unsigned char  bit;
    unsigned char  mask;
    if (idx0 >= REG_DISC_N) return MBUSR_ERR_RANGE;
    byte = (unsigned short)(idx0 / 8);
    bit  = (unsigned char)(idx0 % 8);
    mask = (unsigned char)(1u << bit);
    EnterCriticalSection();
    if (on) ucDiscBuf[byte] |= mask;
    else    ucDiscBuf[byte] &= (unsigned char)(~mask);
    ExitCriticalSection();
    return MBUSR_OK;
}

int MBUSR_GetDiscreteIdx(unsigned short idx0, int *pOn)
{
    unsigned short byte;
    unsigned char  bit;
    unsigned char  mask;
    if (pOn == NULL) return MBUSR_ERR_ARG;
    if (idx0 >= REG_DISC_N) return MBUSR_ERR_RANGE;
    byte = (unsigned short)(idx0 / 8);
    bit  = (unsigned char)(idx0 % 8);
    mask = (unsigned char)(1u << bit);
    EnterCriticalSection();
    *pOn = (ucDiscBuf[byte] & mask) ? 1 : 0;
    ExitCriticalSection();
    return MBUSR_OK;
}

int MBUSR_SetDiscreteByAddr(unsigned short addr1b, int on)
{
    if (addr1b < REG_DISC_START) return MBUSR_ERR_RANGE;
    return MBUSR_SetDiscreteIdx((unsigned short)(addr1b - REG_DISC_START), on);
}

int MBUSR_GetDiscreteByAddr(unsigned short addr1b, int *pOn)
{
    if (addr1b < REG_DISC_START) return MBUSR_ERR_RANGE;
    return MBUSR_GetDiscreteIdx((unsigned short)(addr1b - REG_DISC_START), pOn);
}
