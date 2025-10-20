#include "tcp_server_multi_socket.h"
#include "socket.h"
#include "stdio.h"
#include "wizchip_conf.h"
#include "sys.h"
#define _LOOPBACK_DEBUG_

#define LOCAL_MAX_SOCK   4
	 
uint8_t socket_sn = 0, socket_flag = 0;

/* ---- 新增：链路与每个 socket 的 keepalive 标志 ---- */
static uint8_t s_link_last = 0xFF; /* 未知 */
static uint8_t s_keepalive_set[LOCAL_MAX_SOCK] = {0};


/* ---- 新增：设置 TCP KeepAlive，优先使用 setsockopt，回退到 Sn_KPALVTR ---- */
static void set_tcp_keepalive(uint8_t sn, uint8_t seconds_approx)
{
    /* W5500 的 Sn_KPALVTR 单位是 5 秒，这里做个近似换算 */
    uint8_t ka5s = (seconds_approx + 4) / 5;
    if (ka5s == 0) ka5s = 1;        /* 最小 5s */
    if (ka5s > 0xFF) ka5s = 0xFF;   /* 8bit 上限 */

#if defined(SO_KEEPALIVE)
    /* ioLibrary 的 setsockopt 版本（大多数版本都有） */
    {
        uint8_t ka = ka5s; /* 仍旧传 5 秒基数以与 Sn_KPALVTR 对齐 */
        (void)setsockopt(sn, SO_KEEPALIVE, &ka);
    }
#else
    /* 直接写寄存器（需要 socket.h 暴露 setSn_KPALVTR 或 w5500.h 宏） */
    setSn_KPALVTR(sn, ka5s);
#endif
}

/* ---- 新增：检测 PHY 链路，掉线时关闭全部 socket 做“热插拔自愈” ---- */
static void link_monitor_and_recover(void)
{
    uint8_t link;
    uint8_t i;

    if (ctlwizchip(CW_GET_PHYLINK, (void *)&link) != 0)
        return;

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


/**
 * @brief  multi socket loopback with keepalive + link monitor
 */
int32_t multi_tcps_socket(uint8_t *buf, uint16_t localport)
{
    int32_t ret;
    uint16_t size = 0, sentsize = 0;

#ifdef _LOOPBACK_DEBUG_
    uint8_t destip[4];
    uint16_t destport;
#endif

    /* 先巡检 PHY 链路，链路掉线则不做后续操作 */
    link_monitor_and_recover();
    if (s_link_last == PHY_LINK_OFF) {
        /* 链路断开，略过处理，等恢复后自动重建 */
        goto rotate_only;
    }

    switch (getSn_SR(socket_sn))
    {
    case SOCK_ESTABLISHED:
        /* 处理新连接事件：设置 keepalive，发一字节触发对端 keepalive 路径 */
        if (getSn_IR(socket_sn) & Sn_IR_CON)
        {
#ifdef _LOOPBACK_DEBUG_
            getSn_DIPR(socket_sn, destip);
            destport = getSn_DPORT(socket_sn);
            printf("%d:Connected - %d.%d.%d.%d : %d\r\n",
                   socket_sn, destip[0], destip[1], destip[2], destip[3], destport);
#endif
            if (!s_keepalive_set[socket_sn]) {
                /* 例如 10 秒 keepalive（W5500: 2×5s） */
                set_tcp_keepalive(socket_sn, 10);
                s_keepalive_set[socket_sn] = 1;
            }

            ret = send(socket_sn, (uint8_t *)"\0", 1); /* 触发 keepalive */
            if (ret < 0) {
                close(socket_sn);
                s_keepalive_set[socket_sn] = 0;
                return ret;
            }
            setSn_IR(socket_sn, Sn_IR_CON);
        }

        /* 有数据就回显 */
        size = getSn_RX_RSR(socket_sn);
        if (size > 0)
        {
            if (size > DATA_BUF_SIZE)
                size = DATA_BUF_SIZE;

            ret = recv(socket_sn, buf, size);
            if (ret <= 0) {
                /* 这里可能是 SOCKERR_BUSY 或者错误，直接返回给上层处理 */
                return ret;
            }

            size = (uint16_t)ret;
            sentsize = 0;

            /* 打印方便调试：buf 需要 DATA_BUF_SIZE+1 的空间 */
            buf[size] = 0;
#ifdef _LOOPBACK_DEBUG_
            printf("%d:recv data:%s\r\n", socket_sn, buf);
#endif

            while (sentsize < size)
            {
                ret = send(socket_sn, buf + sentsize, size - sentsize);
                if (ret < 0) {
                    /* 对端异常断开/超时等：主动关闭以便下次重连 */
                    close(socket_sn);
                    s_keepalive_set[socket_sn] = 0;
                    return ret;
                }
                sentsize += (uint16_t)ret;
            }
        }
        break;

    case SOCK_CLOSE_WAIT:
#ifdef _LOOPBACK_DEBUG_
        printf("%d:CloseWait\r\n", socket_sn);
#endif
        ret = disconnect(socket_sn);
        if (ret != SOCK_OK) {
            return ret;
        }
#ifdef _LOOPBACK_DEBUG_
        printf("%d:Socket Closed\r\n", socket_sn);
#endif
        s_keepalive_set[socket_sn] = 0;
        break;

    case SOCK_INIT:
#ifdef _LOOPBACK_DEBUG_
        printf("%d:Listen, TCP server loopback, port [%d]\r\n", socket_sn, localport);
#endif
        ret = listen(socket_sn);
        if (ret != SOCK_OK) {
            return ret;
        }
        break;

    case SOCK_CLOSED:
#ifdef _LOOPBACK_DEBUG_
        printf("%d:TCP server loopback start\r\n", socket_sn);
#endif
        /* 注意：W5500 不支持同一设备多个 socket 绑定同一端口同时监听
           如果你确实需要多连接，请只让 1 个 socket 监听 localport，
           其余 socket 作为客户端或使用不同端口。 */
        ret = socket(socket_sn, Sn_MR_TCP, localport, 0x00);
        if (ret != socket_sn) {
            return ret;
        }
#ifdef _LOOPBACK_DEBUG_
        printf("%d:Socket opened\r\n", socket_sn);
#endif
        s_keepalive_set[socket_sn] = 0; /* 新开清标志，等 ESTABLISHED 再设 */
        break;

    default:
        break;
    }

rotate_only:
    /* 轮转下一个 socket（0..LOCAL_MAX_SOCK-1） */
    socket_sn++;
    if (socket_sn >= LOCAL_MAX_SOCK) {
        socket_sn = 0;
    }
    return 1;
}


