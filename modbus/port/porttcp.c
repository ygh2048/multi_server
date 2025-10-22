/*
 * FreeModbus TCP Port for W5500 multi-socket base (tcp_srv_*)
 * - 支持多 socket 同端口监听
 * - 按 MBAP 长度组整帧后再交给栈
 * - 通过 printf 调试（受 TCP_MULTI_DEBUG 控制）
 */

#include <stdio.h>
#include <string.h>
#include "port.h"
#include "mb.h"
#include "mbport.h"

#include "modtcpbsp.h"   

#ifndef MB_TCP_DEFAULT_PORT
#define MB_TCP_DEFAULT_PORT    502
#endif

/* ========== MBAP 下标 ==========
 * | TransactionID(2) | ProtocolID(2) | Length(2) | UnitID(1) | PDU...
 * 长度字段为网络序，含 UnitID(1) + PDU 长度 -> 整帧总长 = 7 + Length
 */
#define MB_TCP_UID     6
#define MB_TCP_LEN     4
#define MB_TCP_FUNC    7

/* ========== 端口层内部缓存 ==========
 * 你可以按需调大（与 modtcpbsp.h 的 TCP_RX_MAX 相互独立）
 */
#ifndef MB_TCP_BUF_SIZE
#define MB_TCP_BUF_SIZE  (260 + 7) /* 常见 PDU + MBAP 足够 */
#endif

static UCHAR  ucTCPBuf[MB_TCP_BUF_SIZE];
static USHORT usTCPBufLen = 0;

/* 记录“当前处理的连接”，用于 SendResponse 回发给同一 socket */
static UCHAR  ucCurConnSn = 0xFF;

/* -------------------- FreeModbus 端口 API -------------------- */

BOOL xMBTCPPortInit( USHORT usTCPPort )
{
    USHORT port;

    if( usTCPPort == 0 ) {
        port = (USHORT)MB_TCP_DEFAULT_PORT;
    } else {
        port = usTCPPort;
    }

#if TCP_MULTI_DEBUG
    TCP_DBG("[MBTCP] PortInit: port=%u\r\n", (unsigned)port);
#endif

    /* 打开多 socket 并监听同一个端口 */
    tcp_srv_init(port);

    /* 可选：设置 keepalive（W5500 5s 步进，这里 ~10s） */
    tcp_srv_set_keepalive(10);

    /* 置空当前连接与缓存 */
    ucCurConnSn = 0xFF;
    usTCPBufLen = 0;

    return TRUE;
}

void vMBTCPPortClose( void )
{
#if TCP_MULTI_DEBUG
    TCP_DBG("[MBTCP] PortClose\r\n");
#endif
    tcp_srv_deinit();
    ucCurConnSn = 0xFF;
    usTCPBufLen = 0;
}

void vMBTCPPortDisable( void )
{
#if TCP_MULTI_DEBUG
    TCP_DBG("[MBTCP] PortDisable -> close_all\r\n");
#endif
    tcp_srv_close_all();
    ucCurConnSn = 0xFF;
    usTCPBufLen = 0;
}

/* 供你的主循环调用：先驱动底座轮询 */
void vMBPortTCPPool( void )
{
    /* 兼容你原文件的命名，内部做的事情就是底座 poll */
    tcp_srv_poll();
}

BOOL xMBTCPPortGetRequest(UCHAR **ppucMBTCPFrame, USHORT *usTCPLength)
{
    uint8_t sn;
    const uint8_t *p;
    uint16_t plen;
    uint16_t want;
    uint16_t mbap_len;
    uint16_t got;
    int ok;
    int i;

    /* 先驱动底座，让它把数据搬到内部每 socket 的缓冲区 */
    tcp_srv_poll();  // 确保这一步被调用并处理完请求

    ok = tcp_srv_peek(&sn, &p, &plen);
    if (!ok || plen < 7) {
        /* 如果没有收到数据或数据未完成，打印调试信息并退出 */
        TCP_DBG("[MBTCP] No data or MBAP not full (plen=%u)\r\n", plen);
        return FALSE;  /* 没数据 or MBAP 未满 */
    }

    /* 打印调试信息，查看接收到的数据 */
    TCP_DBG("[MBTCP] RX: sn=%u plen=%u\n", (unsigned)sn, plen);
    for (i = 0; i < plen; i++) {
        TCP_DBG("[MBTCP] 0x%02X ", p[i]);
        if ((i + 1) % 16 == 0) {
            TCP_DBG("\n");
        }
    }
    TCP_DBG("\n");

    /* 计算 MBAP 长度（从帧头的 Length 字段读取） */
    mbap_len = ((uint16_t)p[MB_TCP_LEN] << 8) | (uint16_t)p[MB_TCP_LEN + 1];
    want = (uint16_t)(7u + mbap_len);

    if (plen < want) {
        /* 如果数据还不完整，打印调试信息并退出 */
        TCP_DBG("[MBTCP] Incomplete frame. want=%u, plen=%u\r\n", want, plen);
        return FALSE;  /* 还没收齐一整帧 */
    }

    /* 打印调试信息，查看期望的完整帧长度 */
    TCP_DBG("[MBTCP] Full frame received. want=%u, plen=%u\r\n", want, plen);

    /* 读取完整的 Modbus 请求并传递给栈 */
    if (want > MB_TCP_BUF_SIZE) return FALSE;
    if (!tcp_srv_read(&sn, ucTCPBuf, want, &got)) {
        /* 如果读取失败，打印调试信息并退出 */
        TCP_DBG("[MBTCP] tcp_srv_read failed\r\n");
        return FALSE;
    }

    if (got != want) {
        /* 如果实际读取的字节数与期望的不符，打印调试信息 */
        TCP_DBG("[MBTCP] Data mismatch. got=%u, want=%u\r\n", got, want);
        return FALSE;
    }

    /* 打印接收到的完整 Modbus 请求数据 */
    TCP_DBG("[MBTCP] Full request received: sn=%u len=%u func=%u\r\n",
            (unsigned)sn, (unsigned)got, (unsigned)ucTCPBuf[MB_TCP_FUNC]);

    /* 将收到的请求传递给 Modbus 协议栈 */
    ucCurConnSn = sn;
    usTCPBufLen = want;
    *ppucMBTCPFrame = &ucTCPBuf[0];
    *usTCPLength = usTCPBufLen;

    return TRUE;
}

/* 把响应发回“上一次 GetRequest 的那个 socket” */


BOOL xMBTCPPortSendResponse( const UCHAR *pucMBTCPFrame, USHORT usTCPLength )
{
    int ret;

    if (ucCurConnSn == 0xFF) {
#if TCP_MULTI_DEBUG
        TCP_DBG("[MBTCP] SEND: no active conn\r\n");
#endif
        return FALSE;
    }

    ret = tcp_srv_send(ucCurConnSn, pucMBTCPFrame, usTCPLength);

if (ret < 0) {
#if TCP_MULTI_DEBUG
    TCP_DBG("[MBTCP] Send failed. Error code: %d\r\n", ret);
#endif
    return FALSE;
}

#if TCP_MULTI_DEBUG
    TCP_DBG("[MBTCP] RSP sn=%u len=%u ret=%d\r\n",
            (unsigned)ucCurConnSn, (unsigned)usTCPLength, ret);
#endif

    usTCPBufLen = 0;

    return (ret == 0) ? TRUE : FALSE;
}


/* ----------- FreeModbus 需要的临界区封装（保持不变） ----------- */
void EnterCriticalSection( void ) { __disable_irq(); }
void ExitCriticalSection( void )  { __enable_irq();  }

void vMBPortTCPPool(void);         /* 这是你新的轮询函数 */
void xMBPortTCPPool(void) { vMBPortTCPPool(); }
