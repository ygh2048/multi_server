#ifndef TCP_SERVER_SINGLE_SOCKET_H
#define TCP_SERVER_SINGLE_SOCKET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TCP单连接服务器API */
void tcp_srv_single_init(uint16_t port);
void tcp_srv_single_deinit(void);
void tcp_srv_single_poll(void);
int tcp_srv_single_peek(const uint8_t **pbuf, uint16_t *plen);
int tcp_srv_single_read(uint8_t *dst, uint16_t maxlen, uint16_t *outlen);
int tcp_srv_single_send(const uint8_t *src, uint16_t len);
void tcp_srv_single_close(void);
void tcp_srv_single_set_keepalive(uint8_t seconds_approx);
uint8_t tcp_srv_single_is_connected(void);
uint8_t tcp_srv_single_get_client_sn(void);

typedef enum {
    TCP_LINK_UP = 0,
    TCP_LINK_DOWN = 1
} tcp_link_t;

tcp_link_t tcp_srv_single_link_state(void);

#ifdef __cplusplus
}
#endif

#endif /* TCP_SERVER_SINGLE_SOCKET_H */