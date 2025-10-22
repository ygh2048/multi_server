#ifndef _MODTCPBSP_H_
#define _MODTCPBSP_H_

#include <stdint.h>
#include <stddef.h>



/* ================= 调试开关（直接 printf 到串口） ================= */
/* 0: 关闭; 1: 基本日志; 2: 详细日志 */
#ifndef TCP_MULTI_DEBUG
#define TCP_MULTI_DEBUG 2
#endif

#if TCP_MULTI_DEBUG
  #include <stdio.h>
  #define TCP_DBG(...)  printf(__VA_ARGS__)
#else
  #define TCP_DBG(...)  ((void)0)
#endif
/* ================================================================= */

/* ================= 可配置参数 ================= */
#ifndef TCP_SRV_PORT
#define TCP_SRV_PORT       502      /* Modbus/TCP 常用端口，按需改 */
#endif

#ifndef TCP_MAX_SOCK
#define TCP_MAX_SOCK       4        /* 并发 socket 数 (1~8) */
#endif

#ifndef TCP_RX_MAX
#define TCP_RX_MAX         260      /* 单次缓存上限（可按协议需要调整） */
#endif
/* ============================================= */

#ifdef __cplusplus
extern "C" {
#endif

/* PHY 链路状态 */
typedef enum {
    TCP_LINK_DOWN = 0,
    TCP_LINK_UP   = 1
} tcp_link_t;

/* 生命周期 */
void      tcp_srv_init(uint16_t port);          /* 打开 TCP_MAX_SOCK 个 socket 并在同一端口 listen */
void      tcp_srv_deinit(void);                 /* 关闭所有 socket（同时清状态） */

/* 轮询（必须在主循环/任务里持续调用） */
void      tcp_srv_poll(void);

/* 数据面（协议无关） */
int       tcp_srv_peek(uint8_t *sn, const uint8_t **pbuf, uint16_t *plen);
/* 读取并“消费”这段数据（把缓冲清空）。返回1=成功有数据，0=无数据。 */
int       tcp_srv_read(uint8_t *sn, uint8_t *dst, uint16_t maxlen, uint16_t *outlen);
/* 发送到指定 socket。返回0=OK，负值=错误。 */
int       tcp_srv_send(uint8_t sn, const uint8_t *src, uint16_t len);

/* 连接管理 */
void      tcp_srv_close(uint8_t sn);            /* 主动关闭某个连接并重新 listen */
void      tcp_srv_close_all(void);              /* 关闭全部并重新 listen */

/* 其他工具 */
void      tcp_srv_set_keepalive(uint8_t seconds_approx);  /* 约多少秒（W5500 是 5s 步进） */
tcp_link_t tcp_srv_link_state(void);                        /* 当前 PHY 状态 */

#ifdef __cplusplus
}
#endif

#endif /* TCP_MULTI_H */
