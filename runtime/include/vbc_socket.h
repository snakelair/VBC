#ifndef VBC_SOCKET_H
#define VBC_SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include "vbc_runtime.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET vbc_socket_handle_t;
#define VBC_INVALID_SOCKET INVALID_SOCKET
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
typedef int vbc_socket_handle_t;
#define VBC_INVALID_SOCKET (-1)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    vbc_socket_handle_t handle;
    bool is_valid;
} vbc_socket_t;

bool vbc_socket_system_init(void);
void vbc_socket_system_cleanup(void);

vbc_socket_t vbc_socket_create_tcp_server(int port, int backlog);
vbc_socket_t vbc_socket_accept(vbc_socket_t server, char* remote_ip, int remote_ip_len, int* remote_port);
vbc_socket_t vbc_socket_connect_tcp(const char* host, int port, int timeout_ms);

int64_t vbc_socket_send(vbc_socket_t sock, const char* buffer, int64_t len);
int64_t vbc_socket_recv(vbc_socket_t sock, char* buffer, int64_t max_len);

void vbc_socket_close(vbc_socket_t* sock);
void vbc_socket_set_timeout(vbc_socket_t sock, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* VBC_SOCKET_H */
