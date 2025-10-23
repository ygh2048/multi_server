#include "modtcpbsp.h"

/*
 * modtcpbsp.c
 * ---------
 * W5500-based TCP server BSP for Modbus/TCP.
 *
 * 概要：
 * - 提供两种模式：单连接（tcp_srv_single_*）与多连接（tcp_srv_*），由
 *   `modtcpbsp.h` 中的编译时宏 `TCP_MULTI_CONNECTION_MODE` 控制。
 * - 单连接模式：只打开一个 socket（client socket），上层通过 tcp_srv_single_*
 *   轮询/读取数据。资源占用最小，推荐用于常见 Modbus 从站场景。
 * - 多连接模式：打开多个 socket（数量由 TCP_MAX_SOCK 决定），为每个 socket
 *   维护独立状态和接收缓冲区，允许多个客户端并发连接（但组帧逻辑在端口层
 *   仍需处理，每个 socket 有独立累加区）。
 *
 * 数据流（被动拉模型）：
 * - BSP 负责和 W5500 交互（listen/accept/recv/send/close），并把接收到的原始
 *   字节放入 rxbuf 或通过 tcp_srv_peek/read 提供给上层。
 * - 上层（modbus/port/porttcp.c 的 vMBPortTCPPool）周期性调用 tcp_srv_* 的
 *   peek/read，把数据追加到累加缓冲并尝试解析 MBAP 帧；一旦解析出完整帧，
 *   上层会把帧交给 FreeModbus 处理，处理结果再通过 tcp_srv_send/send_single
 *   发回。
 *
 * 日志/调试：
 * - 使用 modtcpbsp.h 中定义的 MODTCP_DBG(level, ...) 宏进行集中控制。
 *   - level 1: 基本事件/错误
 *   - level 2: 详细事件（例如帧十六进制转储）
 * - 在调试时可以把 MODTCP_DEBUG_LEVEL 提高到 2，但生产编译建议保留为 1
 *   或 0，以避免串口被大量十六进制转储刷屏。
 *
 * 内存/性能建议：
 * - TCP_MAX_SOCK 不要超过目标板和 W5500 的能力（通常 <= 8）；每增加一个
 *   socket，会增加一定的 RAM（每 socket 的 rxbuf 和状态结构）。
 * - MB_TCP_ACC_SIZE 控制端口层的累加缓冲大小；设置时请考虑最坏情况下的
 *   单帧大小与同时缓存的帧数。
 */
#include "socket.h"
#include "stdio.h"
#include "wizchip_conf.h"
#include "sys.h"
#include <string.h>



static uint16_t       s_port = TCP_SRV_PORT;  /* 监听端口（统一端口） */
static uint8_t        s_link_last = 0xFF;     /* 上一次 PHY 状态：0xFF=未知 */
static uint8_t        s_ka_5s = 2;            /* keepalive 单位 5s（默认≈10s） */


/* ------------- 内部上下文 ------------- */
typedef struct {
    uint8_t  rxbuf[TCP_RX_MAX];
    uint16_t rxlen;             /* 已收未读的字节数 */
    uint8_t  connected;         /* 是否已建立连接 */
    uint8_t  keepalive_set;     /* 是否已设置过 keepalive */
    uint8_t  client_sn;         /* 客户端socket编号 */
} tcp_single_ctx_t;


/* ------------- 内部上下文 ------------- */
typedef struct {
    uint8_t  rxbuf[TCP_RX_MAX];
    uint16_t rxlen;             /* 已收未读的字节数 */
    uint8_t  connected;         /* 是否已建立连接 */
    uint8_t  keepalive_set;     /* 是否已设置过 keepalive */
} tcp_sock_ctx_t;


static tcp_single_ctx_t s_ctx;
static uint8_t          s_listen_sn = 0;        /* 监听socket固定为0 */

#ifdef TCP_MULTI_CONNECTION_MODE

static tcp_sock_ctx_t s_ctx[TCP_MAX_SOCK];



/* ------------- 内部工具函数 ------------- */
/* 设置/激活 keepalive（W5500: Sn_KPALVTR，单位5s；需先发一个字节激活） */
static void apply_keepalive(uint8_t sn)
{
    if (!s_ctx[sn].keepalive_set) {
        if (s_ka_5s == 0) s_ka_5s = 1;
        setSn_KPALVTR(sn, s_ka_5s);
        /* 发送1字节触发 keepalive 路径 */
        s_ctx[sn].keepalive_set = 1;
#if TCP_MULTI_DEBUG
        TCP_DBG("[TCP] sock%u: keepalive ~%us (step5s=%u)\r\n",
                (unsigned)sn, (unsigned)(s_ka_5s * 5u), (unsigned)s_ka_5s);
#endif
    }
}

/* 监测 PHY 变化：掉线则清空并关闭全部 socket；联上则让 poll 重新打开监听 */
static void monitor_phy(void)
{
    uint8_t link;
    uint8_t i;

    if (ctlwizchip(CW_GET_PHYLINK, (void *)&link) != 0) {
        /* 读取失败就不改状态 */
        return;
    }

    if (s_link_last == 0xFF) {
        s_link_last = link;
#if TCP_MULTI_DEBUG
        TCP_DBG("[TCP] PHY first: %s\r\n", (link == PHY_LINK_ON) ? "UP" : "DOWN");
#endif
        return;
    }

    if (link != s_link_last) {
#if TCP_MULTI_DEBUG
        TCP_DBG("[TCP] PHY: %s -> %s\r\n",
                (s_link_last == PHY_LINK_ON) ? "UP" : "DOWN",
                (link == PHY_LINK_ON) ? "UP" : "DOWN");
#endif
        s_link_last = link;
        if (link == PHY_LINK_OFF) {
            for (i = 0; i < TCP_MAX_SOCK; i++) {
                (void)close(i);
                s_ctx[i].connected     = 0u;
                s_ctx[i].keepalive_set = 0u;
                s_ctx[i].rxlen         = 0u;
            }
        }
    }
}

/* ------------- 对外 API 实现 ------------- */
void tcp_srv_init(uint16_t port)
{
    uint8_t i;

    if (port != 0u) {
        s_port = port;
    } else {
        s_port = TCP_SRV_PORT;
    }

#if TCP_MULTI_DEBUG
    TCP_DBG("\r\n[TCP] init: port=%u, max_sock=%u, rxmax=%u\r\n",
            (unsigned)s_port, (unsigned)TCP_MAX_SOCK, (unsigned)TCP_RX_MAX);
#endif

    for (i = 0u; i < TCP_MAX_SOCK; i++) {
        s_ctx[i].rxlen         = 0u;
        s_ctx[i].connected     = 0u;
        s_ctx[i].keepalive_set = 0u;

        (void)close(i);
        if (socket(i, Sn_MR_TCP, s_port, 0x00) == i) {
            (void)listen(i);
#if TCP_MULTI_DEBUG
            TCP_DBG("[TCP] sock%u: LISTEN on %u\r\n", (unsigned)i, (unsigned)s_port);
#endif
        } else {
#if TCP_MULTI_DEBUG
            TCP_DBG("[TCP] sock%u: socket() FAIL\r\n", (unsigned)i);
#endif
        }
    }

    s_link_last = 0xFFu;  /* 让下一次 poll 输出一次当前 PHY */
}

void tcp_srv_deinit(void)
{
#if TCP_MULTI_DEBUG
    TCP_DBG("[TCP] deinit\r\n");
#endif
    tcp_srv_close_all();
}

void tcp_srv_poll(void)
{
    uint8_t i;
    uint8_t sr;

    /* 先看 PHY；断链则不做后续 */
    monitor_phy();
    if (s_link_last == PHY_LINK_OFF) {
        return;
    }

    /* 驱动每个 socket 的状态机 */
    for (i = 0u; i < TCP_MAX_SOCK; i++) {
        sr = getSn_SR(i);

        if (sr == SOCK_ESTABLISHED) {
            /* 首次进入 ESTABLISHED */
            if (!s_ctx[i].connected) {
                s_ctx[i].connected = 1u;
#if TCP_MULTI_DEBUG
                TCP_DBG("[TCP] sock%u: ESTABLISHED\r\n", (unsigned)i);
#endif
                apply_keepalive(i);
            }

            /* 把片上 RX 拿到软件缓冲（简易做法：每 socket 一段缓存） */
            {
                uint16_t avail;
                avail = getSn_RX_RSR(i);
                while (avail) {
                    uint16_t room;
                    uint16_t chunk;
                    int32_t  r;

                    room = (TCP_RX_MAX > s_ctx[i].rxlen) ? (TCP_RX_MAX - s_ctx[i].rxlen) : 0u;
                    if (room == 0u) {
                        /* 已满：丢弃一部分，避免死锁 */
                        uint8_t tmp[64];
                        chunk = (avail > (uint16_t)sizeof(tmp)) ? (uint16_t)sizeof(tmp) : avail;
                        r = recv(i, tmp, chunk);
#if TCP_MULTI_DEBUG
                        if (r > 0) {
                            TCP_DBG("[TCP] sock%u: DROP %ldB (RX full)\r\n", (unsigned)i, (long)r);
                        }
#endif
                        if (r <= 0) {
                            break;
                        }
                        avail -= (uint16_t)r;
                        continue;
                    }

                    chunk = (avail > room) ? room : avail;
                    r = recv(i, s_ctx[i].rxbuf + s_ctx[i].rxlen, chunk);
                    if (r <= 0) {
                        break;
                    }
                    s_ctx[i].rxlen += (uint16_t)r;
#if TCP_MULTI_DEBUG >= 2
                    TCP_DBG("[TCP] sock%u: RX +%ldB, pend=%u\r\n",
                            (unsigned)i, (long)r, (unsigned)s_ctx[i].rxlen);
#endif
                    avail -= (uint16_t)r;
                }
            }
        }
        else if (sr == SOCK_CLOSE_WAIT) {
#if TCP_MULTI_DEBUG
            TCP_DBG("[TCP] sock%u: CLOSE_WAIT -> disconnect\r\n", (unsigned)i);
#endif
            (void)disconnect(i);
        }
        else if (sr == SOCK_CLOSED) {
            /* 断开->重新开启 LISTEN */
            if (s_ctx[i].connected) {
#if TCP_MULTI_DEBUG
                TCP_DBG("[TCP] sock%u: CLOSED\r\n", (unsigned)i);
#endif
            }
            s_ctx[i].connected     = 0u;
            s_ctx[i].keepalive_set = 0u;
            s_ctx[i].rxlen         = 0u;

            if (socket(i, Sn_MR_TCP, s_port, 0x00) == i) {
                (void)listen(i);
#if TCP_MULTI_DEBUG
                TCP_DBG("[TCP] sock%u: LISTEN again\r\n", (unsigned)i);
#endif
            } else {
#if TCP_MULTI_DEBUG
                TCP_DBG("[TCP] sock%u: reopen FAIL\r\n", (unsigned)i);
#endif
            }
        }
        else if (sr == SOCK_INIT) {
            (void)listen(i);
#if TCP_MULTI_DEBUG
            TCP_DBG("[TCP] sock%u: INIT -> LISTEN\r\n", (unsigned)i);
#endif
        } else {
            /* 其它状态不打印，避免刷屏 */
        }
    }
}

int tcp_srv_peek(uint8_t *sn, const uint8_t **pbuf, uint16_t *plen)
{
    uint8_t i;
    for (i = 0u; i < TCP_MAX_SOCK; i++) {
        if (s_ctx[i].rxlen > 0u) {
            if (sn   != NULL) *sn   = i;
            if (pbuf != NULL) *pbuf = s_ctx[i].rxbuf;
            if (plen != NULL) *plen = s_ctx[i].rxlen;
#if TCP_MULTI_DEBUG >= 2
            TCP_DBG("[TCP] sock%u: PEEK %uB\r\n", (unsigned)i, (unsigned)s_ctx[i].rxlen);
#endif
            return 1; /* 有数据 */
        }
    }
    return 0; /* 无数据 */
}

int tcp_srv_read(uint8_t *sn, uint8_t *dst, uint16_t maxlen, uint16_t *outlen)
{
    uint8_t  sid;
    const uint8_t *p;
    uint16_t n;
    uint16_t c;

    if (!tcp_srv_peek(&sid, &p, &n)) {
        return 0; /* 无数据 */
    }

    c = (n > maxlen) ? maxlen : n;
    if (dst != NULL && c > 0u) {
        memcpy(dst, p, c);
    }

    s_ctx[sid].rxlen = 0u; /* 消费掉 */
    if (sn     != NULL) *sn     = sid;
    if (outlen != NULL) *outlen = c;

#if TCP_MULTI_DEBUG
    TCP_DBG("[TCP] sock%u: READ %uB\r\n", (unsigned)sid, (unsigned)c);
#endif
    return 1; /* 成功读到 */
}

int tcp_srv_send(uint8_t sn, const uint8_t *src, uint16_t len)
{
    int32_t sent;
    if (sn >= TCP_MAX_SOCK) {
        return -1; /* 参数非法 */
    }
    if (!s_ctx[sn].connected) {
        return -2; /* 未连接 */
    }

#if TCP_MULTI_DEBUG
    TCP_DBG("[TCP] sock%u: SEND %uB\r\n", (unsigned)sn, (unsigned)len);
#endif

    sent = 0;
    while (sent < (int32_t)len) {
        int32_t r;
        r = send(sn, (uint8_t*)src + sent, (uint16_t)(len - (uint16_t)sent));
        if (r < 0) {
#if TCP_MULTI_DEBUG
            TCP_DBG("[TCP] sock%u: send ERR=%ld -> close\r\n", (unsigned)sn, (long)r);
#endif
            (void)close(sn);
            s_ctx[sn].connected     = 0u;
            s_ctx[sn].keepalive_set = 0u;
            s_ctx[sn].rxlen         = 0u;
            return -3;
        }
        sent += r;
    }
    return 0; /* OK */
}

void tcp_srv_close(uint8_t sn)
{
    if (sn >= TCP_MAX_SOCK) {
        return;
    }
#if TCP_MULTI_DEBUG
    TCP_DBG("[TCP] sock%u: CLOSE by app\r\n", (unsigned)sn);
#endif
    (void)close(sn);
    s_ctx[sn].connected     = 0u;
    s_ctx[sn].keepalive_set = 0u;
    s_ctx[sn].rxlen         = 0u;

    /* 关闭后立即恢复监听 */
    if (socket(sn, Sn_MR_TCP, s_port, 0x00) == sn) {
        (void)listen(sn);
    }
}

void tcp_srv_close_all(void)
{
    uint8_t i;
#if TCP_MULTI_DEBUG
    TCP_DBG("[TCP] CLOSE_ALL\r\n");
#endif
    for (i = 0u; i < TCP_MAX_SOCK; i++) {
        tcp_srv_close(i);
    }
}

void tcp_srv_set_keepalive(uint8_t seconds_approx)
{
    uint8_t k;
    k = (uint8_t)((seconds_approx + 4u) / 5u);  /* 5s 步进 */
    if (k == 0u) {
        k = 1u;
    }
    s_ka_5s = k;
#if TCP_MULTI_DEBUG
    TCP_DBG("[TCP] keepalive set: ~%us (step5s=%u)\r\n",
            (unsigned)(s_ka_5s * 5u), (unsigned)s_ka_5s);
#endif
}

tcp_link_t tcp_srv_link_state(void)
{
    uint8_t link;
    if (ctlwizchip(CW_GET_PHYLINK, (void*)&link) != 0) {
        /* 失败时不改变状态；按 UP 返回，避免误判 */
        return TCP_LINK_UP;
    }
    return (link == PHY_LINK_ON) ? TCP_LINK_UP : TCP_LINK_DOWN;
}


#endif

#ifndef TCP_MULTI_CONNECTION_MODE

/* ------------- 内部工具函数 ------------- */
/* 设置/激活 keepalive */

static void apply_keepalive_single(uint8_t sn)
{
    if (!s_ctx.keepalive_set) {
        if (s_ka_5s == 0) s_ka_5s = 1;
        setSn_KPALVTR(sn, s_ka_5s);
        s_ctx.keepalive_set = 1;
#if TCP_SINGLE_DEBUG
        TCP_DBG("[TCP-SINGLE] sock%u: keepalive ~%us (step5s=%u)\r\n",
                (unsigned)sn, (unsigned)(s_ka_5s * 5u), (unsigned)s_ka_5s);
#endif
    }
}



/* 监测 PHY 变化 */
static void monitor_phy_single(void)
{
    uint8_t link;

    if (ctlwizchip(CW_GET_PHYLINK, (void *)&link) != 0) {
        /* 读取失败就不改状态 */
        return;
    }

    if (s_link_last == 0xFF) {
        s_link_last = link;
#if TCP_SINGLE_DEBUG
        TCP_DBG("[TCP-SINGLE] PHY first: %s\r\n", (link == PHY_LINK_ON) ? "UP" : "DOWN");
#endif
        return;
    }

    if (link != s_link_last) {
#if TCP_SINGLE_DEBUG
        TCP_DBG("[TCP-SINGLE] PHY: %s -> %s\r\n",
                (s_link_last == PHY_LINK_ON) ? "UP" : "DOWN",
                (link == PHY_LINK_ON) ? "UP" : "DOWN");
#endif
        s_link_last = link;
        if (link == PHY_LINK_OFF) {
            /* 断网时关闭连接 */
            (void)close(s_listen_sn);
            if (s_ctx.connected) {
                (void)close(s_ctx.client_sn);
            }
            s_ctx.connected     = 0u;
            s_ctx.keepalive_set = 0u;
            s_ctx.rxlen         = 0u;
        }
    }
}

/* ------------- 对外 API 实现 ------------- */
void tcp_srv_single_init(uint16_t port)
{
    if (port != 0u) {
        s_port = port;
    } else {
        s_port = TCP_SRV_PORT;
    }

    /* 初始化上下文 */
    s_ctx.rxlen         = 0u;
    s_ctx.connected     = 0u;
    s_ctx.keepalive_set = 0u;
    s_ctx.client_sn     = 0xFF;

#if TCP_SINGLE_DEBUG
    TCP_DBG("\r\n[TCP-SINGLE] init: port=%u, rxmax=%u\r\n",
            (unsigned)s_port, (unsigned)TCP_RX_MAX);
#endif

    /* 关闭可能的旧连接 */
    (void)close(s_listen_sn);
    
    /* 创建监听socket */
    if (socket(s_listen_sn, Sn_MR_TCP, s_port, 0x00) == s_listen_sn) {
        (void)listen(s_listen_sn);
#if TCP_SINGLE_DEBUG
        TCP_DBG("[TCP-SINGLE] LISTEN on %u\r\n", (unsigned)s_port);
#endif
    } else {
#if TCP_SINGLE_DEBUG
        TCP_DBG("[TCP-SINGLE] socket() FAIL\r\n");
#endif
    }

    s_link_last = 0xFFu;
}

void tcp_srv_single_deinit(void)
{
#if TCP_SINGLE_DEBUG
    TCP_DBG("[TCP-SINGLE] deinit\r\n");
#endif
    tcp_srv_single_close();
}

void tcp_srv_single_poll(void)
{
    uint8_t sr;
    uint16_t chunk;
    int32_t r;
    uint16_t room ;
    uint16_t avail;
    /* 先看 PHY；断链则不做后续 */
    monitor_phy_single();
    if (s_link_last == PHY_LINK_OFF) {
        return;
    }

    /* 检查监听socket状态 */
    sr = getSn_SR(s_listen_sn);

    if (sr == SOCK_ESTABLISHED) {
        /* 有客户端连接建立 */
        if (!s_ctx.connected) {
            s_ctx.connected = 1u;
            s_ctx.client_sn = s_listen_sn;
#if TCP_MULTI_DEBUG
            TCP_DBG("[TCP-SINGLE] New connection established\r\n");
#endif
            apply_keepalive_single(s_ctx.client_sn);
        }

        /* 处理接收数据：尽可能读取所有可用字节，并按 MB/TCP (MBAP) 解析完整帧 */
        avail = getSn_RX_RSR(s_ctx.client_sn);
        while (avail > 0) {
            room = (uint16_t)((TCP_RX_MAX > s_ctx.rxlen) ? (TCP_RX_MAX - s_ctx.rxlen) : 0u);

            if (room == 0u) {
                /* 缓冲区已满：记录并断开连接以避免协议状态不一致（也可改为环形缓冲策略） */
#if TCP_MULTI_DEBUG
                TCP_DBG("[TCP-SINGLE] ERROR: RX buffer overflow (pend=%u), closing client\r\n", (unsigned)s_ctx.rxlen);
#endif
                tcp_srv_single_close();
                return;
            }

            chunk = (avail > room) ? room : avail;
            r = recv(s_ctx.client_sn, s_ctx.rxbuf + s_ctx.rxlen, chunk);
            if (r <= 0) {
                /* recv 失败或无更多数据 */
                break;
            }
            s_ctx.rxlen = (uint16_t)(s_ctx.rxlen + r);
#if TCP_MULTI_DEBUG
            TCP_DBG("[TCP-SINGLE] RX +%ldB, pend=%u\r\n", (long)r, (unsigned)s_ctx.rxlen);
#endif
            avail = (uint16_t)(avail - (uint16_t)r);

            /* 被动拉模式：只把数据放到 s_ctx.rxbuf，具体组帧/解析由上层 porttcp.c 的 vMBPortTCPPool() 来处理 */
            /* s_ctx.rxlen 已经更新，上层的 tcp_srv_single_peek / tcp_srv_single_read 会读取这些数据 */
        } /* end while (avail) */
    }
}

int tcp_srv_single_peek(const uint8_t **pbuf, uint16_t *plen)
{
    if (s_ctx.rxlen > 0u) {
        if (pbuf != NULL) *pbuf = s_ctx.rxbuf;
        if (plen != NULL) *plen = s_ctx.rxlen;
#if TCP_SINGLE_DEBUG >= 2
        TCP_DBG("[TCP-SINGLE] PEEK %uB\r\n", (unsigned)s_ctx.rxlen);
#endif
        return 1; /* 有数据 */
    }
    return 0; /* 无数据 */
}
int tcp_srv_single_read(uint8_t *dst, uint16_t maxlen, uint16_t *outlen)
{
    const uint8_t *p;
    uint16_t n;
    uint16_t c;

    if (!tcp_srv_single_peek(&p, &n)) {
        return 0; /* 无数据 */
    }

    c = (n > maxlen) ? maxlen : n;

    if (c > 0u && dst != NULL) {
        memcpy(dst, p, c);
    }

    if (c < n) {
        /* 仅消费前 c 字节，把剩余 n-c 前移保留，避免丢包/半包 */
        memmove(s_ctx.rxbuf, s_ctx.rxbuf + c, (size_t)(n - c));
        s_ctx.rxlen = (uint16_t)(n - c);
    } else {
        s_ctx.rxlen = 0u;
    }

    if (outlen != NULL) *outlen = c;

#if TCP_SINGLE_DEBUG >= 2
    TCP_DBG("[TCP-SINGLE] READ %u/%u (remain=%u)\r\n",
            (unsigned)c, (unsigned)n, (unsigned)s_ctx.rxlen);
#endif
    return 1;
}




int tcp_srv_single_send(const uint8_t *src, uint16_t len)
{
    int32_t sent;
    
    if (!s_ctx.connected) {
        return -2; /* 未连接 */
    }

#if TCP_SINGLE_DEBUG
    TCP_DBG("[TCP-SINGLE] SEND %uB\r\n", (unsigned)len);
#endif

    sent = 0;
    while (sent < (int32_t)len) {
        int32_t r;
        r = send(s_ctx.client_sn, (uint8_t*)src + sent, (uint16_t)(len - (uint16_t)sent));
        if (r < 0) {
#if TCP_SINGLE_DEBUG
            TCP_DBG("[TCP-SINGLE] send ERR=%ld -> close\r\n", (long)r);
#endif
            tcp_srv_single_close();
            return -3;
        }
        sent += r;
    }
    return 0; /* OK */
}

void tcp_srv_single_close(void)
{
#if TCP_SINGLE_DEBUG
    TCP_DBG("[TCP-SINGLE] CLOSE by app\r\n");
#endif
    
    if (s_ctx.connected) {
        (void)close(s_ctx.client_sn);
    }
    
    s_ctx.connected     = 0u;
    s_ctx.keepalive_set = 0u;
    s_ctx.rxlen         = 0u;
    s_ctx.client_sn     = 0xFF;

    /* 关闭后立即恢复监听 */
    (void)close(s_listen_sn);
    if (socket(s_listen_sn, Sn_MR_TCP, s_port, 0x00) == s_listen_sn) {
        (void)listen(s_listen_sn);
    }
}

void tcp_srv_single_set_keepalive(uint8_t seconds_approx)
{
    uint8_t k;
    k = (uint8_t)((seconds_approx + 4u) / 5u);  /* 5s 步进 */
    if (k == 0u) {
        k = 1u;
    }
    s_ka_5s = k;
#if TCP_SINGLE_DEBUG
    TCP_DBG("[TCP-SINGLE] keepalive set: ~%us (step5s=%u)\r\n",
            (unsigned)(s_ka_5s * 5u), (unsigned)s_ka_5s);
#endif
}

tcp_link_t tcp_srv_single_link_state(void)
{
    uint8_t link;
    if (ctlwizchip(CW_GET_PHYLINK, (void*)&link) != 0) {
        /* 失败时不改变状态；按 UP 返回，避免误判 */
        return TCP_LINK_UP;
    }
    return (link == PHY_LINK_ON) ? TCP_LINK_UP : TCP_LINK_DOWN;
}

/* 获取当前连接状态 */
uint8_t tcp_srv_single_is_connected(void)
{
    return s_ctx.connected;
}

/* 获取客户端socket号 */
uint8_t tcp_srv_single_get_client_sn(void)
{
    return s_ctx.client_sn;
}


/* 获取缓冲区使用情况 */
uint16_t tcp_srv_single_get_rx_buffer_usage(void)
{
    return s_ctx.rxlen;
}

uint16_t tcp_srv_single_get_rx_buffer_size(void)
{
    return TCP_RX_MAX;
}

#endif






















