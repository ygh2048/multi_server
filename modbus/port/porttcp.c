/* ==== Modbus/TCP port: minimal linear-accumulator framer (C90) ==== */
#include <stdio.h>
#include <string.h>
#include "port.h"
#include "mb.h"
#include "mbport.h"
#include "modtcpbsp.h"  /* tcp_srv_single_* */

#define TCP_DEBUG 1
#define TCP_DEBUG_PREFIX "[MBTCP-SIMPLE]"

/* Modbus TCP: 7B MBAP + PDU(<=253) -> 单帧最大 ≈ 260B */
#ifndef MB_TCP_FRAME_MAX
#define MB_TCP_FRAME_MAX  260
#endif

/* 线性累加缓冲：能同时容纳数帧即可，太大无意义。*/
#ifndef MB_TCP_ACC_SIZE
#define MB_TCP_ACC_SIZE   (MB_TCP_FRAME_MAX * 3) /* 780B */
#endif

/* MBAP 索引 */
#define MB_TCP_TID_H 0
#define MB_TCP_TID_L 1
#define MB_TCP_PID_H 2
#define MB_TCP_PID_L 3
#define MB_TCP_LEN_H 4
#define MB_TCP_LEN_L 5
#define MB_TCP_UID   6
#define MB_TCP_FUNC  7

/* ---- 对 FreeModbus 暴露的整帧缓冲 ---- */
static UCHAR  ucTCPBuf[MB_TCP_FRAME_MAX];
static USHORT usTCPBufLen;
static volatile UCHAR s_frame_ready;

/* ---- 线性累加缓冲（唯一写入点） ---- */
static UCHAR  s_acc[MB_TCP_ACC_SIZE];
static USHORT s_acc_len;

/* ---- 工具：校验 MBAP 并算总长 ---- */
static int mbap_ok_and_total(const UCHAR *h7, USHORT *p_total)
{
    USHORT pid, len, total;
    pid = (USHORT)((((USHORT)h7[MB_TCP_PID_H]) << 8) | (USHORT)h7[MB_TCP_PID_L]);
    if (pid != 0) {
#if TCP_DEBUG
        printf("%s PID invalid: 0x%04X\r\n", TCP_DEBUG_PREFIX, (unsigned)pid);
#endif
        return 0;
    }
    len = (USHORT)((((USHORT)h7[MB_TCP_LEN_H]) << 8) | (USHORT)h7[MB_TCP_LEN_L]);
    /* len 至少包含1字节 UnitId；且 6+len 不得超过单帧上限 */
    if (len < 1U || (USHORT)(6U + len) > MB_TCP_FRAME_MAX) {
#if TCP_DEBUG
        printf("%s LEN invalid: %u\r\n", TCP_DEBUG_PREFIX, (unsigned)len);
#endif
        return 0;
    }
    total = (USHORT)(6U + len);
    *p_total = total;
    return 1;
}

/* ---- 把 TCP 数据 append 到累加缓冲（唯一入口） ---- */
static void pump_tcp_to_acc(void)
{
    const UCHAR *p;
    USHORT plen;
    USHORT got;
    UCHAR tmp[256];
    USHORT chunk;
    int ok;

    for (;;) {
        ok = tcp_srv_single_peek(&p, &plen);
        if (!ok || plen == 0U) break;

        while (plen > 0U) {
            chunk = (plen > (USHORT)sizeof(tmp)) ? (USHORT)sizeof(tmp) : plen;
            if (!tcp_srv_single_read(tmp, chunk, &got)) return;
            if (got == 0U) return;

            /* 空间不够就清空累加缓冲（防御式处理，宁可丢数据不崩溃） */
            if ((USHORT)(s_acc_len + got) > MB_TCP_ACC_SIZE) {
#if TCP_DEBUG
                printf("%s ACC overflow -> reset (acc=%u, got=%u)\r\n",
                       TCP_DEBUG_PREFIX, (unsigned)s_acc_len, (unsigned)got);
#endif
                s_acc_len = 0U;
                s_frame_ready = 0U;
            }

            /* append */
            memcpy(&s_acc[s_acc_len], tmp, got);
            s_acc_len = (USHORT)(s_acc_len + got);

#if TCP_DEBUG
            { USHORT show = (got > 24U) ? 24U : got; USHORT i;
              printf("%s APPEND %uB: ", TCP_DEBUG_PREFIX, (unsigned)got);
              for (i = 0; i < show; i++) printf("%02X ", (unsigned)tmp[i]);
              printf("\r\n");
            }
#endif

            if (plen >= got) plen = (USHORT)(plen - got);
            else plen = 0U;
        }
    }
}

/* ---- 在累加缓冲里组帧；可一次组多帧，但只对外暴露“下一帧” ---- */
static void try_extract_frames(void)
{
    USHORT total;

    /* 外面还没取走上一帧，就不继续了 */
    if (s_frame_ready) return;

    /* 尝试连续取帧 */
    for (;;) {
        if (s_acc_len < 7U) return; /* 不够一个 MBAP */

        /* 判头：如果不合法，丢1字节重试 */
        if (!mbap_ok_and_total(&s_acc[0], &total)) {
            /* slide-1 */
            memmove(&s_acc[0], &s_acc[1], (size_t)(s_acc_len - 1U));
            s_acc_len--;
            continue;
        }

        /* 需要更多数据 */
        if (s_acc_len < total) return;

        /* 抽出一帧交给 FreeModbus */
        memcpy(ucTCPBuf, &s_acc[0], total);
        usTCPBufLen   = total;
        s_frame_ready = 1U;

#if TCP_DEBUG
        { USHORT i, show = (total > 32U) ? 32U : total;
          printf("%s FRAME READY %uB: ", TCP_DEBUG_PREFIX, (unsigned)total);
          for (i = 0; i < show; i++) printf("%02X ", (unsigned)ucTCPBuf[i]);
          printf("\r\n");
        }
#endif

        /* 从累加缓冲中移除这帧 */
        if (s_acc_len > total) {
            memmove(&s_acc[0], &s_acc[total], (size_t)(s_acc_len - total));
            s_acc_len = (USHORT)(s_acc_len - total);
        } else {
            s_acc_len = 0U;
        }

        /* 只暴露“一帧”，让上层及时取走；剩余帧下次再交 */
        return;
    }
}

/* ---- FreeModbus 端口 API ---- */
BOOL xMBTCPPortInit(USHORT usTCPPort)
{
    USHORT port = (usTCPPort == 0U) ? (USHORT)502 : usTCPPort;

#if TCP_DEBUG
    printf("%s PortInit: %u\r\n", TCP_DEBUG_PREFIX, (unsigned)port);
#endif

    tcp_srv_single_init(port);
    tcp_srv_single_set_keepalive(10);

    s_acc_len     = 0U;
    s_frame_ready = 0U;
    usTCPBufLen   = 0U;
    return TRUE;
}

void vMBTCPPortClose(void)
{
#if TCP_DEBUG
    printf("%s PortClose\r\n", TCP_DEBUG_PREFIX);
#endif
    tcp_srv_single_deinit();
    s_acc_len     = 0U;
    s_frame_ready = 0U;
    usTCPBufLen   = 0U;
}

void vMBTCPPortDisable(void)
{
#if TCP_DEBUG
    printf("%s PortDisable\r\n", TCP_DEBUG_PREFIX);
#endif
    tcp_srv_single_close();
    s_acc_len     = 0U;
    s_frame_ready = 0U;
    usTCPBufLen   = 0U;
}

/* 由上层周期调用（或你原先的 vMBPortTCPPool 包装）：驱动TCP+组帧 */
void vMBPortTCPPool(void)
{
    tcp_srv_single_poll();   /* 仅此处读TCP */
    pump_tcp_to_acc();       /* append 到线性累加缓冲 */
    try_extract_frames();    /* 从累加缓冲里抽帧 */
}

BOOL xMBTCPPortGetRequest(UCHAR **ppucMBTCPFrame, USHORT *usTCPLength)
{
    /* 兜底推进一次，避免上层轮询慢时卡住 */
    vMBPortTCPPool();

    if (!s_frame_ready) return FALSE;
    *ppucMBTCPFrame = ucTCPBuf;
    *usTCPLength    = usTCPBufLen;

    /* 标记被取走，允许抽下一帧 */
    s_frame_ready = 0U;
    return TRUE;
}

/* 直接把响应写回同一 socket（单连接方案） */
BOOL xMBTCPPortSendResponse(const UCHAR *pucMBTCPFrame, USHORT usTCPLength)
{
    int ret;
#if TCP_DEBUG
    printf("%s SEND %uB\r\n", TCP_DEBUG_PREFIX, (unsigned)usTCPLength);
#endif
    ret = tcp_srv_single_send(pucMBTCPFrame, usTCPLength);
    return (ret == 0) ? TRUE : FALSE;
}

/* 临界区（照旧） */
void EnterCriticalSection(void) { __disable_irq(); }
void ExitCriticalSection(void)  { __enable_irq();  }

/* 兼容函数名（若上层调用这个） */
void xMBPortTCPPool(void) { vMBPortTCPPool(); }
