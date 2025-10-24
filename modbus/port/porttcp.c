/*
 * porttcp.c
 * ---------
 * Modbus/TCP 端口层（Framer）—— 线性累加缓冲实现（兼容 C90）
 *
 * 设计说明：
 * - 使用被动拉（poll）模型：BSP 负责 socket 的读写（recv/send），
 *   端口层周期性调用 vMBPortTCPPool() 来驱动读取、追加到累加缓冲
 *   并尝试解析完整的 MBAP+PDU 帧。
 * - 单连接模式（当前文件）：使用一个全局累加缓冲 s_acc。多连接逻辑在
 *   需要时由另一个分支/实现提供（per-socket 小缓冲）。
 * - 错误处理采用保守策略：当检测到异常头部或缓冲溢出时，按策略丢弃
 *   有问题的字节或重置累加缓冲，而不是造成崩溃。
 *
 * 重要概念：
 * - MBAP 头部长度为 7 字节（TID(2) PID(2) LEN(2) UID(1)），其中 PID 必须为 0。
 * - 整帧长度 = 6 + LEN（LEN 中包含 UnitId 的长度）。
 * - 配置项见 modtcpbsp.h: MB_TCP_FRAME_MAX, MB_TCP_ACC_SIZE。
 *
 * 调试与日志：
 * - 代码中使用 TCP_DEBUG/TCP_DEBUG_PREFIX 输出本地日志；在其他实现分支中，
 *   也可能使用 MODTCP_DBG 宏以集中控制日志级别。避免在生产环境打印
 *   大量十六进制转储以免串口刷屏。
 */
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
static volatile USHORT s_acc_len;  /* volatile 避免优化，确保调试时能看到实际值 */
static volatile USHORT s_last_acc_len; /* 上次的累加长度，用于检测突变 */

/* 在每次写 s_acc_len 前检查边界 */
static void check_acc_len(const char* where, USHORT new_len)
{
    EnterCriticalSection();
    if (new_len > MB_TCP_ACC_SIZE || s_acc_len > MB_TCP_ACC_SIZE || 
        (new_len > (s_last_acc_len + MB_TCP_FRAME_MAX))) {
        /* 检测到异常，重置所有状态 */
        memset(s_acc, 0, sizeof(s_acc));
        s_acc_len = 0;
        s_last_acc_len = 0;
        s_frame_ready = 0;
    }
    ExitCriticalSection();
}

/* ---- 工具：校验 MBAP 并算总长 ---- */
static int mbap_ok_and_total(const UCHAR *h7, USHORT *p_total)
{
    USHORT tid, pid, len, total;
    tid = (USHORT)((((USHORT)h7[MB_TCP_TID_H]) << 8) | (USHORT)h7[MB_TCP_TID_L]);
    pid = (USHORT)((((USHORT)h7[MB_TCP_PID_H]) << 8) | (USHORT)h7[MB_TCP_PID_L]);
    if (pid != 0) {
#if TCP_DEBUG
        printf("%s TID=0x%04X PID invalid: 0x%04X\r\n", 
               TCP_DEBUG_PREFIX, (unsigned)tid, (unsigned)pid);
#endif
        return 0;
    }
    len = (USHORT)((((USHORT)h7[MB_TCP_LEN_H]) << 8) | (USHORT)h7[MB_TCP_LEN_L]);
    /* len 至少包含1字节 UnitId；且 6+len 不得超过单帧上限 */
    if (len < 1U || (USHORT)(6U + len) > MB_TCP_FRAME_MAX) {
#if TCP_DEBUG
        printf("%s TID=0x%04X LEN invalid: %u\r\n", 
               TCP_DEBUG_PREFIX, (unsigned)tid, (unsigned)len);
#endif
        return 0;
    }
    total = (USHORT)(6U + len);
    *p_total = total;
#if TCP_DEBUG
    printf("%s TID=0x%04X len=%u total=%u\r\n",
           TCP_DEBUG_PREFIX, (unsigned)tid, (unsigned)len, (unsigned)total);
#endif
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

    /* 进入临界区 */
    EnterCriticalSection();

    /* 增强的状态检查：检查溢出和可疑的长度突变 */
    if (s_acc_len > MB_TCP_ACC_SIZE ||  /* 累加缓冲区溢出 */
        (s_acc_len > 0 && s_acc_len == s_last_acc_len && s_frame_ready == 0)) { /* 可能的死锁 */
#if TCP_DEBUG
        printf("%s WARNING: acc_len=%u last=%u ready=%u max=%u\r\n",
               TCP_DEBUG_PREFIX, (unsigned)s_acc_len, 
               (unsigned)s_last_acc_len, (unsigned)s_frame_ready,
               (unsigned)MB_TCP_ACC_SIZE);
#endif
        /* 完全重置状态 */
        memset(s_acc, 0, sizeof(s_acc));
        s_acc_len = 0;
        s_frame_ready = 0;
        s_last_acc_len = 0;
        ExitCriticalSection();
        return;  /* 跳过本次读取 */
    }
    s_last_acc_len = s_acc_len;
    ExitCriticalSection();

    for (;;) {
        ok = tcp_srv_single_peek(&p, &plen);
        if (!ok || plen == 0U) break;

        while (plen > 0U) {
            chunk = (plen > (USHORT)sizeof(tmp)) ? (USHORT)sizeof(tmp) : plen;
            if (!tcp_srv_single_read(tmp, chunk, &got)) return;
            if (got == 0U) return;

            /* 空间不够就清空累加缓冲（防御式处理，宁可丢数据不崩溃） */
            if (s_acc_len > MB_TCP_ACC_SIZE || got > MB_TCP_ACC_SIZE || 
                (USHORT)(s_acc_len + got) > MB_TCP_ACC_SIZE) {
#if TCP_DEBUG
                printf("%s ACC overflow -> reset (acc=%u, got=%u, max=%u)\r\n",
                       TCP_DEBUG_PREFIX, (unsigned)s_acc_len, (unsigned)got,
                       (unsigned)MB_TCP_ACC_SIZE);
#endif
                /* 完全重置状态 */
                memset(s_acc, 0, sizeof(s_acc));
                s_acc_len = 0U;
                s_frame_ready = 0U;
                return;  /* 中止本次读取 */
            }

            /* append：经过上面检查，这里的操作应该是安全的 */
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

    /* 外面还没取走上一帧，就不继续了，但要输出诊断信息 */
    if (s_frame_ready) {
#if TCP_DEBUG
        printf("%s frame_ready=1, waiting for FreeModbus...\r\n", 
               TCP_DEBUG_PREFIX);
#endif
        return;
    }

    /* 尝试连续取帧 */
    for (;;) {
        if (s_acc_len < 7U) return; /* 不够一个 MBAP */

        /* 判头：如果不合法，丢1字节重试 */
        if (!mbap_ok_and_total(&s_acc[0], &total)) {
            /* slide-1，但先检查长度避免下溢 */
            if (s_acc_len <= 1) {
                s_acc_len = 0;
                return;
            }
            memmove(&s_acc[0], &s_acc[1], (size_t)(s_acc_len - 1U));
            s_acc_len--;
#if TCP_DEBUG
            printf("%s slide-1: new len=%u\r\n", 
                   TCP_DEBUG_PREFIX, (unsigned)s_acc_len);
#endif
            continue;
        }

            /* 需要更多数据 */
        if (s_acc_len < total) return;

        /* 抽出一帧交给 FreeModbus */
        memcpy(ucTCPBuf, &s_acc[0], total);
        usTCPBufLen   = total;
        s_frame_ready = 1U;

        /* 通知上层 FreeModbus 有帧到达（事件驱动），使 eMBPoll 能处理该帧 */
#if TCP_DEBUG
        if (!xMBPortEventPost(EV_FRAME_RECEIVED)) {
            printf("%s Frame event post FAILED\r\n", TCP_DEBUG_PREFIX);
        } else {
            printf("%s Frame event posted\r\n", TCP_DEBUG_PREFIX);
        }
#else
        (void)xMBPortEventPost(EV_FRAME_RECEIVED);
#endif

				
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

    EnterCriticalSection();
    
    tcp_srv_single_init(port);
    tcp_srv_single_set_keepalive(10);

    /* 确保正确初始化所有状态 */
    memset(s_acc, 0, sizeof(s_acc));
    s_acc_len     = 0U;
    s_last_acc_len = 0U;  /* 重置累加长度记录 */
    s_frame_ready = 0U;
    usTCPBufLen   = 0U;
    
    ExitCriticalSection();
#if TCP_DEBUG
    printf("%s Init: acc_size=%u frame_max=%u\r\n",
           TCP_DEBUG_PREFIX, (unsigned)MB_TCP_ACC_SIZE,
           (unsigned)MB_TCP_FRAME_MAX);
#endif
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
    /* Diagnostic: print MBAP tid/uid/len so we can confirm the frame handed to stack */
    TCP_DBG("[MBTCP-SIMPLE] GETREQ TID=%02X%02X UID=%02X LEN=%u\r\n",
            (unsigned)ucTCPBuf[MB_TCP_TID_H], (unsigned)ucTCPBuf[MB_TCP_TID_L],
            (unsigned)ucTCPBuf[MB_TCP_UID], (unsigned)usTCPBufLen);
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
    TCP_DBG("[MBTCP-SIMPLE] SEND result=%d\r\n", (int)ret);
    return (ret == 0) ? TRUE : FALSE;
}

/* 临界区（照旧） */
void EnterCriticalSection(void) { __disable_irq(); }
void ExitCriticalSection(void)  { __enable_irq();  }

/* 兼容函数名（若上层调用这个） */
void xMBPortTCPPool(void) { vMBPortTCPPool(); }
