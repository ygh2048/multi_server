#ifndef MB_USER_REG_H
#define MB_USER_REG_H

#include "mb.h"
#include "mbport.h"   /* ??? EnterCriticalSection/ExitCriticalSection */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ? mb_user_reg.c ???? —— 1-based ?????? */
#define REG_HOLDING_START   1
#define REG_HOLDING_N       100

#define REG_INPUT_START     1
#define REG_INPUT_N         100

#define REG_COILS_START     1
#define REG_COILS_N         64

#define REG_DISC_START      1
#define REG_DISC_N          64

/* ????(??) */
void  MBUSR_Init(void);  /* ????????,?? .c ??? */

/* ========== Holding Registers (RW, 16-bit) ========== */
int   MBUSR_SetHoldingByAddr(unsigned short addr1b, unsigned short val);
int   MBUSR_GetHoldingByAddr(unsigned short addr1b, unsigned short *pVal);

/* 0-based ????(????????) */
int   MBUSR_SetHoldingIdx(unsigned short idx0, unsigned short val);
int   MBUSR_GetHoldingIdx(unsigned short idx0, unsigned short *pVal);

/* ========== Input Registers (RO, 16-bit) ========== */
int   MBUSR_SetInputByAddr(unsigned short addr1b, unsigned short val); /* ??????? */
int   MBUSR_GetInputByAddr(unsigned short addr1b, unsigned short *pVal);

int   MBUSR_SetInputIdx(unsigned short idx0, unsigned short val);
int   MBUSR_GetInputIdx(unsigned short idx0, unsigned short *pVal);

/* ========== Coils (RW, 1-bit) ========== */
int   MBUSR_SetCoilByAddr(unsigned short addr1b, int on);
int   MBUSR_GetCoilByAddr(unsigned short addr1b, int *pOn);

int   MBUSR_SetCoilIdx(unsigned short idx0, int on);
int   MBUSR_GetCoilIdx(unsigned short idx0, int *pOn);

/* ========== Discrete Inputs (RO, 1-bit) ========== */
int   MBUSR_SetDiscreteByAddr(unsigned short addr1b, int on);  /* ??????? */
int   MBUSR_GetDiscreteByAddr(unsigned short addr1b, int *pOn);

int   MBUSR_SetDiscreteIdx(unsigned short idx0, int on);
int   MBUSR_GetDiscreteIdx(unsigned short idx0, int *pOn);

/* ?? 0 ????;<0 ????????? */
#define MBUSR_OK              (0)
#define MBUSR_ERR_RANGE      (-1)
#define MBUSR_ERR_ARG        (-2)

#ifdef __cplusplus
}
#endif

#endif /* MB_USER_REG_H */
