#include "tcp_server_multi_socket.h"
#include "socket.h"
#include "stdio.h"
#include "wizchip_conf.h"
#include "sys.h"

#define _LOOPBACK_DEBUG_

/* ================== 配置区 ================== */
#ifndef LOCAL_MAX_SOCK
#define LOCAL_MAX_SOCK   4
#endif

#ifndef DATA_BUF_SIZE
/* 缓冲区缺省值。根据你的 ioLibrary 配置（每路 RX/TX 缓冲）可调整。 */
#define DATA_BUF_SIZE    1460
#endif

/* 某些库未定义时的兜底常量 */
#ifndef PHY_LINK_ON
#define PHY_LINK_ON  1
#endif
#ifndef PHY_LINK_OFF
#define PHY_LINK_OFF 0
#endif
#ifndef SOCKERR_BUSY
#define SOCKERR_BUSY (-7)
#endif

#ifdef _WIZCHIP_SOCK_NUM_
  #if (LOCAL_MAX_SOCK > _WIZCHIP_SOCK_NUM_)
    #error "LOCAL_MAX_SOCK 大于芯片可用 socket 数。请下调 LOCAL_MAX_SOCK 或调整缓冲配置。"
  #endif
#endif
/* =========================================== */

/* 轮询索引与标志（延用你的命名与可见性） */
uint8_t socket_sn = 0, socket_flag = 0;

/* 链路与每个 socket 的 keepalive 标志 */
static uint8_t s_link_last = 0xFF; /* 未知 */
static uint8_t s_keepalive_set[LOCAL_MAX_SOCK] = {0};

/* ---------- KeepAlive 设置（优先 setsockopt，回退寄存器） ---------- */
static void set_tcp_keepalive(uint8_t sn, uint8_t seconds_approx)
{
    /* W5500 的 Sn_KPALVTR 单位为 5 秒 */
    uint8_t ka5s;

    if (seconds_approx == 0) {
        seconds_approx = 1;
    }
    ka5s = (uint8_t)((seconds_approx + 4) / 5);
    if (ka5s == 0) {
        ka5s = 1; /* 最小 5s */
    }

#if defined(SO_KEEPALIVE)
    {
        uint8_t ka = ka5s; /* 仍旧以 5s 为基数 */
        (void)setsockopt(sn, SO_KEEPALIVE, &ka);
    }
#else
    setSn_KPALVTR(sn, ka5s);
#endif
}

/* ---------- 链路监测 + 自愈 ---------- */
static void link_monitor_and_recover(void)
{
    uint8_t link;
    uint8_t i;
    int rv;

    rv = ctlwizchip(CW_GET_PHYLINK, (void *)&link);
    if (rv != 0) {
        return;
    }

    if (s_link_last == 0xFF) {
        s_link_last = link;
        return;
    }

    if (link != s_link_last) {
#ifdef _LOOPBACK_DEBUG_
        printf("PHY link change: %s -> %s\r\n",
               (s_link_last == PHY_LINK_ON) ? "UP" : "DOWN",
               (link == PHY_LINK_ON) ? "UP" : "DOWN");
#endif
        s_link_last = link;

        if (link == PHY_LINK_OFF) {
            /* 断网：主动关闭所有 socket，并清掉 keepalive 已设标志 */
            for (i = 0; i < LOCAL_MAX_SOCK; i++) {
                if (getSn_SR(i) != SOCK_CLOSED) {
                    close(i);
                }
                s_keepalive_set[i] = 0;
            }
        } else {
            /* 恢复联网：交给主循环在 SOCK_CLOSED 分支重建 */
        }
    }
}

/* ========== 主循环：多 socket TCP server ========== */
int32_t multi_tcps_socket(uint8_t *buf, uint16_t localport)
{
    int32_t ret;
    uint16_t size;
    uint16_t sentsize;

#ifdef _LOOPBACK_DEBUG_
    uint8_t destip[4];
    uint16_t destport;
#endif

    /* 基本健壮性 */
    if (buf == 0) {
#ifdef _LOOPBACK_DEBUG_
        printf("multi_tcps_socket: buf is NULL\r\n");
#endif
        return -1;
    }

    /* 1) 先巡检 PHY 链路 */
    link_monitor_and_recover();

    /* 2) 若链路断开：本轮不处理，直接轮转返回（无 goto） */
    if (s_link_last == PHY_LINK_OFF) {
        socket_sn = (uint8_t)((socket_sn + 1) % LOCAL_MAX_SOCK);
        return 1;
    }

    /* 3) 状态机处理。C90 下如需声明局部变量，必须把每个 case 包进独立块。 */
    switch (getSn_SR(socket_sn))
    {
    case SOCK_ESTABLISHED:
    {
        /* a) 新连接事件：设置 keepalive，并发一字节“点亮”对端 keepalive 路径 */
        if (getSn_IR(socket_sn) & Sn_IR_CON)
        {
            int32_t poke;

#ifdef _LOOPBACK_DEBUG_
            getSn_DIPR(socket_sn, destip);
            destport = getSn_DPORT(socket_sn);
            printf("%d:Connected - %d.%d.%d.%d : %d\r\n",
                   socket_sn, destip[0], destip[1], destip[2], destip[3], destport);
#endif
            if (!s_keepalive_set[socket_sn]) {
                set_tcp_keepalive(socket_sn, 10); /* 约 10s（2×5s） */
                s_keepalive_set[socket_sn] = 1;
            }

            poke = send(socket_sn, (uint8_t *)"\0", 1);
            if (poke < 0) {
                close(socket_sn);
                s_keepalive_set[socket_sn] = 0;
                socket_sn = (uint8_t)((socket_sn + 1) % LOCAL_MAX_SOCK);
                return poke;
            }
            setSn_IR(socket_sn, Sn_IR_CON);
        }

        /* b) 有数据就回显 */
        size = getSn_RX_RSR(socket_sn);
        if (size > 0)
        {
            if (size > DATA_BUF_SIZE) {
                size = DATA_BUF_SIZE;
            }

            ret = recv(socket_sn, buf, size);
            if (ret <= 0) {
                /* 可能是 SOCKERR_BUSY 或者错误，原样返回给上层处理 */
                socket_sn = (uint8_t)((socket_sn + 1) % LOCAL_MAX_SOCK);
                return ret;
            }

            size = (uint16_t)ret;
            sentsize = 0;

            /* 仅调试方便：确保 buf 有 DATA_BUF_SIZE+1 的空间再写 0 结尾 */
            if (size < (DATA_BUF_SIZE + 1)) {
                buf[size] = 0;
#ifdef _LOOPBACK_DEBUG_
                printf("%s", buf);
#endif
            }

            while (sentsize < size)
            {
                ret = send(socket_sn, (uint8_t *)(buf + sentsize), (uint16_t)(size - sentsize));
                if (ret < 0) {
                    /* 对端异常断开/超时等：主动关闭以便下次重连 */
                    close(socket_sn);
                    s_keepalive_set[socket_sn] = 0;
                    socket_sn = (uint8_t)((socket_sn + 1) % LOCAL_MAX_SOCK);
                    return ret;
                }
                sentsize = (uint16_t)(sentsize + (uint16_t)ret);
            }
        }
        break;
    }

    case SOCK_CLOSE_WAIT:
    {
#ifdef _LOOPBACK_DEBUG_
        printf("%d:CloseWait\r\n", socket_sn);
#endif
        ret = disconnect(socket_sn);
        if (ret != SOCK_OK) {
            socket_sn = (uint8_t)((socket_sn + 1) % LOCAL_MAX_SOCK);
            return ret;
        }
#ifdef _LOOPBACK_DEBUG_
        printf("%d:Socket Closed\r\n", socket_sn);
#endif
        s_keepalive_set[socket_sn] = 0;
        break;
    }

    case SOCK_INIT:
    {
#ifdef _LOOPBACK_DEBUG_
        printf("%d:Listen, TCP server loopback, port [%d]\r\n", socket_sn, localport);
#endif
        ret = listen(socket_sn);
        if (ret != SOCK_OK) {
            socket_sn = (uint8_t)((socket_sn + 1) % LOCAL_MAX_SOCK);
            return ret;
        }
        break;
    }

    case SOCK_CLOSED:
    {
#ifdef _LOOPBACK_DEBUG_
        printf("%d:TCP server loopback start\r\n", socket_sn);
#endif
        /* 关键点：允许多个 socket 在**同一个本地端口**打开并进入监听，
           这样每个 socket 能各自承接一条连接，从而实现“同端口多连接”。 */
        ret = socket(socket_sn, Sn_MR_TCP, localport, 0x00);
        if (ret != socket_sn) {
            socket_sn = (uint8_t)((socket_sn + 1) % LOCAL_MAX_SOCK);
            return ret;
        }
#ifdef _LOOPBACK_DEBUG_
        printf("%d:Socket opened\r\n", socket_sn);
#endif
        s_keepalive_set[socket_sn] = 0; /* 新开清标志，等 ESTABLISHED 再设 */
        break;
    }

    default:
        /* 其他状态（如 LISTEN/SYN_RECV 等）不额外处理，直接轮转 */
        break;
    }

    socket_sn = (uint8_t)((socket_sn + 1) % LOCAL_MAX_SOCK);
    return 1;
}
