#ifndef _MODTCPBSP_H_
#define _MODTCPBSP_H_

#include <stdint.h>
#include <stddef.h>

/*
 * MOD_TCP_BSP 说明（摘要）
 *
 * 这个头文件控制 Modbus/TCP 的 BSP（基于 W5500）的编译时行为。
 * 项目中提供两种实现模式：
 *   1) 单连接模式（single connection）—— 上层假设只有一个客户端连接，
 *      BSP 提供 tcp_srv_single_* 系列接口；端口层（porttcp.c）使用单个
 *      累加缓冲区来组帧。这种实现节省资源并且在多数 Modbus 从站场景
 *      下非常适用。
 *
 *   2) 多连接模式（multi connection）—— BSP 打开 TCP_MAX_SOCK 个 socket，
 *      提供 tcp_srv_* 多连接接口，端口层可以为每个 socket 保留一个小累
 *      加区，允许多个客户端同时连接并发请求。代价是代码/数据占用上升。
 *
 * 切换方式（编译时）：
 *   - 仅需通过条件编译宏控制，在本文件中：
 *       #define TCP_MULTI_CONNECTION_MODE   // 启用多连接
 *     或注释该行以使用单连接模式。
 *   - 切换后需要重新编译固件。
 *
 * 重要宏与影响：
 *   - TCP_MAX_SOCK: 并发 socket 数（多连接模式有效）。W5500 最多 8 个 socket，
 *     请不要超过芯片/板子资源限制（通常 <= 8）。更大的值会占用更多 RAM。
 *   - TCP_RX_MAX / MB_TCP_ACC_SIZE: 控制单次/累加缓冲大小。增加会提升能同时
 *     缓冲/组帧的能力但消耗更多 RAM。
 *   - MODTCP_DEBUG / MODTCP_DEBUG_LEVEL: 集中控制 BSP/port 的 printf 日志级别（0/1/2）。
 *
 * 设计注意事项：
 *   - 当前实现使用编译时选择（条件编译）来决定代码路径：这是最简单、最省
 *     资源的做法；如果未来需要运行时切换，可以在 BSP 中添加运行时模式开关和
 *     统一的转发接口，但会增加代码/数据复杂度。
 *   - 多连接模式下，上层（modbus port）需要考虑如何把响应发回正确的 socket，
 *     当前 porttcp.c 使用 s_frame_sock 记录来源 socket（最小侵入方案）。
 *
 * 使用示例（默认单连接）：
 *   - 修改宏使能多连接：在本文件去掉注释 #define TCP_MULTI_CONNECTION_MODE
 *   - 设置并发数：调整 TCP_MAX_SOCK
 *   - 重新编译项目
 */

/* ================= 连接模式选择 ================= */
/* 注释下一行使用单连接模式，取消注释使用多连接模式 */
//#define TCP_MULTI_CONNECTION_MODE

/* ================= 调试开关（直接 printf 到串口） ================= */
/* 0: 关闭; 1: 基本日志; 2: 详细日志 */
#ifndef TCP_MULTI_DEBUG
#define TCP_MULTI_DEBUG 0

#define  TCP_SINGLE_DEBUG 0
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

/* ================= 多连接模式函数声明 ================= */
#ifdef TCP_MULTI_CONNECTION_MODE

/* 生命周期 */
void      tcp_srv_init(uint16_t port);          /* 打开 TCP_MAX_SOCK 个 socket 并在同一端口 listen */
void      tcp_srv_deinit(void);                 /* 关闭所有 socket（同时清状态） */

/* 轮询（必须在主循环/任务里持续调用） */
void      tcp_srv_poll(void);

/* 数据面（协议无关） */
int       tcp_srv_peek(uint8_t *sn, const uint8_t **pbuf, uint16_t *plen);
/* 读取并"消费"这段数据（把缓冲清空）。返回1=成功有数据，0=无数据。 */
int       tcp_srv_read(uint8_t *sn, uint8_t *dst, uint16_t maxlen, uint16_t *outlen);
/* 发送到指定 socket。返回0=OK，负值=错误。 */
int       tcp_srv_send(uint8_t sn, const uint8_t *src, uint16_t len);

/* 连接管理 */
void      tcp_srv_close(uint8_t sn);            /* 主动关闭某个连接并重新 listen */
void      tcp_srv_close_all(void);              /* 关闭全部并重新 listen */

/* 其他工具 */
void      tcp_srv_set_keepalive(uint8_t seconds_approx);  /* 约多少秒（W5500 是 5s 步进） */
tcp_link_t tcp_srv_link_state(void);                        /* 当前 PHY 状态 */

/* ================= 单连接模式函数声明 ================= */
#else

/* 单连接模式接口：上层通过 vMBPortTCPPool() / tcp_srv_single_poll() 被动拉取数据 */
/* 生命周期 */
void      tcp_srv_single_init(uint16_t port);   /* 初始化单连接TCP服务器 */
void      tcp_srv_single_deinit(void);          /* 关闭TCP服务器 */

/* 轮询（必须在主循环/任务里持续调用） */
void      tcp_srv_single_poll(void);

/* 数据面（协议无关） */
int       tcp_srv_single_peek(const uint8_t **pbuf, uint16_t *plen);
/* 读取并"消费"这段数据（把缓冲清空）。返回1=成功有数据，0=无数据。 */
int       tcp_srv_single_read(uint8_t *dst, uint16_t maxlen, uint16_t *outlen);
/* 发送数据到当前连接。返回0=OK，负值=错误。 */
int       tcp_srv_single_send(const uint8_t *src, uint16_t len);

/* 连接管理 */
void      tcp_srv_single_close(void);           /* 主动关闭当前连接并重新监听 */

/* 其他工具 */
void      tcp_srv_single_set_keepalive(uint8_t seconds_approx);  /* 约多少秒（W5500 是 5s 步进） */
tcp_link_t tcp_srv_single_link_state(void);                      /* 当前 PHY 状态 */

/* 连接状态查询 */
uint8_t   tcp_srv_single_is_connected(void);    /* 返回1=已连接，0=未连接 */
uint8_t   tcp_srv_single_get_client_sn(void);   /* 获取当前客户端socket编号 */


uint16_t tcp_srv_single_get_rx_buffer_usage(void);
uint16_t tcp_srv_single_get_rx_buffer_size(void);


#endif /* TCP_MULTI_CONNECTION_MODE */

#ifdef __cplusplus
}
#endif

#endif 


