#include "../include/vbc_socket.h"
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#ifdef _WIN32
static bool s_winsock_initialized = false;
#endif

bool vbc_socket_system_init(void) {
#ifdef _WIN32
    if (!s_winsock_initialized) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            return false;
        }
        s_winsock_initialized = true;
    }
#endif
    return true;
}

void vbc_socket_system_cleanup(void) {
#ifdef _WIN32
    if (s_winsock_initialized) {
        WSACleanup();
        s_winsock_initialized = false;
    }
#endif
}

vbc_socket_t vbc_socket_create_tcp_server(int port, int backlog) {
    vbc_socket_t result = { VBC_INVALID_SOCKET, false };
    if (!vbc_socket_system_init()) return result;

    vbc_socket_handle_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == VBC_INVALID_SOCKET) return result;

    int opt = 1;
#ifdef _WIN32
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        return result;
    }

    if (listen(fd, backlog > 0 ? backlog : 128) != 0) {
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        return result;
    }

    result.handle = fd;
    result.is_valid = true;
    return result;
}

vbc_socket_t vbc_socket_accept(vbc_socket_t server, char* remote_ip, int remote_ip_len, int* remote_port) {
    vbc_socket_t client = { VBC_INVALID_SOCKET, false };
    if (!server.is_valid) return client;

    struct sockaddr_in client_addr;
#ifdef _WIN32
    int addr_len = sizeof(client_addr);
#else
    socklen_t addr_len = sizeof(client_addr);
#endif
    memset(&client_addr, 0, sizeof(client_addr));

    vbc_socket_handle_t fd = accept(server.handle, (struct sockaddr*)&client_addr, &addr_len);
    if (fd == VBC_INVALID_SOCKET) return client;

    if (remote_ip && remote_ip_len > 0) {
        const char* ip_str = inet_ntop(AF_INET, &client_addr.sin_addr, remote_ip, remote_ip_len);
        if (!ip_str) remote_ip[0] = '\0';
    }
    if (remote_port) {
        *remote_port = ntohs(client_addr.sin_port);
    }

    client.handle = fd;
    client.is_valid = true;
    return client;
}

vbc_socket_t vbc_socket_connect_tcp(const char* host, int port, int timeout_ms) {
    vbc_socket_t result = { VBC_INVALID_SOCKET, false };
    if (!vbc_socket_system_init() || !host) return result;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints, *res = NULL, *p = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        return result;
    }

    vbc_socket_handle_t fd = VBC_INVALID_SOCKET;
    for (p = res; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd == VBC_INVALID_SOCKET) continue;

        if (timeout_ms > 0) {
#ifdef _WIN32
            DWORD tv = (DWORD)timeout_ms;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
        }

        if (connect(fd, p->ai_addr, (int)p->ai_addrlen) == 0) {
            break; // connected successfully
        }

#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        fd = VBC_INVALID_SOCKET;
    }

    freeaddrinfo(res);

    if (fd != VBC_INVALID_SOCKET) {
        result.handle = fd;
        result.is_valid = true;
    }
    return result;
}

int64_t vbc_socket_send(vbc_socket_t sock, const char* buffer, int64_t len) {
    if (!sock.is_valid || !buffer || len <= 0) return 0;
    int64_t total_sent = 0;
    while (total_sent < len) {
        int to_send = (int)(len - total_sent);
        int sent = send(sock.handle, buffer + total_sent, to_send, 0);
        if (sent <= 0) break;
        total_sent += sent;
    }
    return total_sent;
}

int64_t vbc_socket_recv(vbc_socket_t sock, char* buffer, int64_t max_len) {
    if (!sock.is_valid || !buffer || max_len <= 0) return 0;
    int bytes = recv(sock.handle, buffer, (int)max_len, 0);
    return bytes >= 0 ? bytes : -1;
}

void vbc_socket_close(vbc_socket_t* sock) {
    if (sock && sock->is_valid) {
#ifdef _WIN32
        closesocket(sock->handle);
#else
        close(sock->handle);
#endif
        sock->handle = VBC_INVALID_SOCKET;
        sock->is_valid = false;
    }
}

void vbc_socket_set_timeout(vbc_socket_t sock, int timeout_ms) {
    if (!sock.is_valid) return;
#ifdef _WIN32
    DWORD tv = (DWORD)timeout_ms;
    setsockopt(sock.handle, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock.handle, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock.handle, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock.handle, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}
