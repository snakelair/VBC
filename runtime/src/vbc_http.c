#include "../include/vbc_http.h"
#include "../include/vbc_async.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool str_case_equals(const char* a, int64_t a_len, const char* b) {
    size_t b_len = strlen(b);
    if ((size_t)a_len != b_len) return false;
    for (size_t i = 0; i < b_len; i++) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    }
    return true;
}

vbc_string_t vbc_http_get_header(const vbc_slice_t* headers, const char* name) {
    if (!headers || !headers->data || !name) return vbc_str_empty();
    for (int64_t i = 0; i < headers->len; i++) {
        vbc_http_header_t* h = (vbc_http_header_t*)vbc_slice_get(headers, i);
        if (str_case_equals(h->name.data, h->name.len, name)) {
            return h->value;
        }
    }
    return vbc_str_empty();
}

/* ========================================================================= */
/* URL Parser                                                                */
/* ========================================================================= */

typedef struct {
    char host[256];
    int port;
    char path[1024];
} parsed_url_t;

static bool parse_http_url(const char* url, int64_t len, parsed_url_t* out) {
    if (!url || len <= 0 || !out) return false;
    memset(out, 0, sizeof(*out));
    out->port = 80;

    const char* p = url;
    const char* end = url + len;

    if (len >= 7 && strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else if (len >= 8 && strncmp(p, "https://", 8) == 0) {
        p += 8;
        out->port = 443;
    }

    const char* host_start = p;
    while (p < end && *p != '/' && *p != ':' && *p != '?') {
        p++;
    }
    size_t host_len = (size_t)(p - host_start);
    if (host_len == 0 || host_len >= sizeof(out->host)) return false;
    memcpy(out->host, host_start, host_len);
    out->host[host_len] = '\0';

    if (p < end && *p == ':') {
        p++;
        int port = 0;
        while (p < end && isdigit((unsigned char)*p)) {
            port = port * 10 + (*p - '0');
            p++;
        }
        if (port > 0) out->port = port;
    }

    if (p < end && *p == '/') {
        size_t path_len = (size_t)(end - p);
        if (path_len >= sizeof(out->path)) path_len = sizeof(out->path) - 1;
        memcpy(out->path, p, path_len);
        out->path[path_len] = '\0';
    } else {
        strcpy(out->path, "/");
    }

    return true;
}

/* ========================================================================= */
/* HTTP Parser Helpers                                                       */
/* ========================================================================= */

static const char* find_crlf_crlf(const char* buf, int64_t len) {
    for (int64_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
            return buf + i;
        }
    }
    return NULL;
}

static void parse_headers(vbc_stack_t* stack, const char* header_start, const char* header_end, vbc_slice_t* headers) {
    const char* line = header_start;
    while (line < header_end) {
        const char* next_line = strstr(line, "\r\n");
        if (!next_line || next_line > header_end) next_line = header_end;

        const char* colon = (const char*)memchr(line, ':', (size_t)(next_line - line));
        if (colon) {
            int64_t name_len = colon - line;
            const char* val_start = colon + 1;
            while (val_start < next_line && (*val_start == ' ' || *val_start == '\t')) {
                val_start++;
            }
            int64_t val_len = next_line - val_start;

            vbc_http_header_t hdr;
            hdr.name = vbc_str_alloc(stack, name_len);
            memcpy((char*)hdr.name.data, line, (size_t)name_len);

            hdr.value = vbc_str_alloc(stack, val_len);
            memcpy((char*)hdr.value.data, val_start, (size_t)val_len);

            vbc_slice_push(stack, headers, &hdr);
        }

        if (next_line + 2 <= header_end) {
            line = next_line + 2;
        } else {
            break;
        }
    }
}

/* ========================================================================= */
/* HTTP Client Implementation                                                */
/* ========================================================================= */

vbc_http_response_t vbc_http_client_get(vbc_stack_t* stack, vbc_string_t url) {
    vbc_http_response_t res;
    memset(&res, 0, sizeof(res));
    res.headers = vbc_slice_create(stack, 8, sizeof(vbc_http_header_t));

    parsed_url_t parsed;
    if (!parse_http_url(url.data, url.len, &parsed)) {
        res.status_code = -1;
        res.status_text = vbc_str_lit("Invalid URL");
        return res;
    }

    vbc_socket_t sock = vbc_socket_connect_tcp(parsed.host, parsed.port, 5000);
    if (!sock.is_valid) {
        res.status_code = -1;
        res.status_text = vbc_str_lit("Connection failed");
        return res;
    }

    char req_buf[2048];
    int req_len = snprintf(req_buf, sizeof(req_buf),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: VBC-HttpClient/1.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n",
        parsed.path, parsed.host, parsed.port);

    vbc_socket_send(sock, req_buf, req_len);

    char recv_buf[8192];
    int64_t total_recv = 0;
    vbc_slice_t raw_bytes = vbc_slice_create(stack, 4096, 1);

    while (true) {
        int64_t bytes = vbc_socket_recv(sock, recv_buf, sizeof(recv_buf));
        if (bytes <= 0) break;
        for (int64_t i = 0; i < bytes; i++) {
            vbc_slice_push(stack, &raw_bytes, &recv_buf[i]);
        }
        total_recv += bytes;
    }
    vbc_socket_close(&sock);

    if (total_recv == 0) {
        res.status_code = -1;
        res.status_text = vbc_str_lit("Empty response");
        return res;
    }

    const char* raw = (const char*)raw_bytes.data;
    const char* header_end = find_crlf_crlf(raw, total_recv);
    if (!header_end) {
        res.status_code = -1;
        res.status_text = vbc_str_lit("Malformed HTTP response");
        return res;
    }

    // Parse status line: HTTP/1.1 200 OK
    int status = 0;
    char status_text_buf[64] = {0};
    sscanf(raw, "HTTP/%*s %d %63[^\r\n]", &status, status_text_buf);
    res.status_code = status;
    res.status_text = vbc_str_lit(status_text_buf[0] ? status_text_buf : "OK");

    const char* first_crlf = strstr(raw, "\r\n");
    if (first_crlf && first_crlf < header_end) {
        parse_headers(stack, first_crlf + 2, header_end, &res.headers);
    }

    const char* body_start = header_end + 4;
    int64_t body_len = total_recv - (body_start - raw);
    res.body = vbc_str_alloc(stack, body_len);
    memcpy((char*)res.body.data, body_start, (size_t)body_len);

    return res;
}

vbc_http_response_t vbc_http_client_post(vbc_stack_t* stack, vbc_string_t url, vbc_string_t body, vbc_string_t content_type) {
    vbc_http_response_t res;
    memset(&res, 0, sizeof(res));
    res.headers = vbc_slice_create(stack, 8, sizeof(vbc_http_header_t));

    parsed_url_t parsed;
    if (!parse_http_url(url.data, url.len, &parsed)) {
        res.status_code = -1;
        res.status_text = vbc_str_lit("Invalid URL");
        return res;
    }

    vbc_socket_t sock = vbc_socket_connect_tcp(parsed.host, parsed.port, 5000);
    if (!sock.is_valid) {
        res.status_code = -1;
        res.status_text = vbc_str_lit("Connection failed");
        return res;
    }

    const char* ct = (content_type.len > 0 && content_type.data) ? content_type.data : "text/plain";
    char req_buf[2048];
    int req_len = snprintf(req_buf, sizeof(req_buf),
        "POST %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "User-Agent: VBC-HttpClient/1.0\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %lld\r\n"
        "Connection: close\r\n\r\n",
        parsed.path, parsed.host, parsed.port, ct, (long long)body.len);

    vbc_socket_send(sock, req_buf, req_len);
    if (body.len > 0 && body.data) {
        vbc_socket_send(sock, body.data, body.len);
    }

    char recv_buf[8192];
    int64_t total_recv = 0;
    vbc_slice_t raw_bytes = vbc_slice_create(stack, 4096, 1);

    while (true) {
        int64_t bytes = vbc_socket_recv(sock, recv_buf, sizeof(recv_buf));
        if (bytes <= 0) break;
        for (int64_t i = 0; i < bytes; i++) {
            vbc_slice_push(stack, &raw_bytes, &recv_buf[i]);
        }
        total_recv += bytes;
    }
    vbc_socket_close(&sock);

    if (total_recv == 0) {
        res.status_code = -1;
        res.status_text = vbc_str_lit("Empty response");
        return res;
    }

    const char* raw = (const char*)raw_bytes.data;
    const char* header_end = find_crlf_crlf(raw, total_recv);
    if (!header_end) {
        res.status_code = -1;
        res.status_text = vbc_str_lit("Malformed HTTP response");
        return res;
    }

    int status = 0;
    char status_text_buf[64] = {0};
    sscanf(raw, "HTTP/%*s %d %63[^\r\n]", &status, status_text_buf);
    res.status_code = status;
    res.status_text = vbc_str_lit(status_text_buf[0] ? status_text_buf : "OK");

    const char* first_crlf = strstr(raw, "\r\n");
    if (first_crlf && first_crlf < header_end) {
        parse_headers(stack, first_crlf + 2, header_end, &res.headers);
    }

    const char* body_start = header_end + 4;
    int64_t body_len = total_recv - (body_start - raw);
    res.body = vbc_str_alloc(stack, body_len);
    memcpy((char*)res.body.data, body_start, (size_t)body_len);

    return res;
}

typedef struct {
    vbc_string_t url;
    vbc_string_t body;
    vbc_string_t content_type;
} _vbc_http_client_args_t;

static void _vbc_http_client_get_fiber(void* _task_param) {
    vbc_task_t* cur = (vbc_task_t*)_task_param;
    _vbc_http_client_args_t* args = (_vbc_http_client_args_t*)cur->args;
    vbc_http_response_t res = vbc_http_client_get(&cur->data_stack, args->url);
    vbc_task_complete(cur, &res, sizeof(res));
}

struct vbc_task* vbc_http_client_get_async(vbc_stack_t* stack, vbc_string_t url) {
    (void)stack;
    _vbc_http_client_args_t args = { url, vbc_str_empty(), vbc_str_empty() };
    return vbc_task_create(_vbc_http_client_get_fiber, &args, sizeof(args));
}

static void _vbc_http_client_get_str_fiber(void* _task_param) {
    vbc_task_t* cur = (vbc_task_t*)_task_param;
    _vbc_http_client_args_t* args = (_vbc_http_client_args_t*)cur->args;
    vbc_http_response_t res = vbc_http_client_get(&cur->data_stack, args->url);
    vbc_task_complete_str(cur, res.body);
}

struct vbc_task* vbc_http_client_get_string_async(vbc_stack_t* stack, vbc_string_t url) {
    (void)stack;
    _vbc_http_client_args_t args = { url, vbc_str_empty(), vbc_str_empty() };
    return vbc_task_create(_vbc_http_client_get_str_fiber, &args, sizeof(args));
}

static void _vbc_http_client_post_fiber(void* _task_param) {
    vbc_task_t* cur = (vbc_task_t*)_task_param;
    _vbc_http_client_args_t* args = (_vbc_http_client_args_t*)cur->args;
    vbc_http_response_t res = vbc_http_client_post(&cur->data_stack, args->url, args->body, args->content_type);
    vbc_task_complete(cur, &res, sizeof(res));
}

struct vbc_task* vbc_http_client_post_async(vbc_stack_t* stack, vbc_string_t url, vbc_string_t body, vbc_string_t content_type) {
    (void)stack;
    _vbc_http_client_args_t args = { url, body, content_type };
    return vbc_task_create(_vbc_http_client_post_fiber, &args, sizeof(args));
}

/* ========================================================================= */
/* HTTP Server Implementation                                                */
/* ========================================================================= */

vbc_http_server_t* vbc_http_server_create(int port) {
    vbc_http_server_t* s = (vbc_http_server_t*)malloc(sizeof(vbc_http_server_t));
    if (!s) return NULL;
    memset(s, 0, sizeof(*s));
    s->port = port;
    vbc_stack_init(&s->req_stack, 16 * 1024 * 1024); // 16 MB request arena
    return s;
}

void vbc_http_server_on(vbc_http_server_t* server, const char* method, const char* path, vbc_http_handler_fn handler) {
    if (!server || server->route_count >= VBC_MAX_ROUTES || !path || !handler) return;
    server->routes[server->route_count].method = method ? method : "GET";
    server->routes[server->route_count].path = path;
    server->routes[server->route_count].handler = handler;
    server->route_count++;
}

void vbc_http_respond(vbc_socket_t client_sock, vbc_stack_t* stack, int status_code, const char* status_text, const char* content_type, vbc_string_t body) {
    (void)stack;
    if (!status_text) {
        status_text = (status_code == 200) ? "OK" :
                      (status_code == 201) ? "Created" :
                      (status_code == 404) ? "Not Found" :
                      (status_code == 500) ? "Internal Server Error" : "Unknown";
    }
    if (!content_type) content_type = "text/plain";

    char header_buf[1024];
    int hlen = snprintf(header_buf, sizeof(header_buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s; charset=utf-8\r\n"
        "Content-Length: %lld\r\n"
        "Server: VBC-HttpServer/1.0\r\n"
        "Connection: close\r\n\r\n",
        status_code, status_text, content_type, (long long)body.len);

    vbc_socket_send(client_sock, header_buf, hlen);
    if (body.len > 0 && body.data) {
        vbc_socket_send(client_sock, body.data, body.len);
    }
}

void vbc_http_respond_text(vbc_socket_t client_sock, vbc_stack_t* stack, int status_code, vbc_string_t text) {
    vbc_http_respond(client_sock, stack, status_code, NULL, "text/plain", text);
}

void vbc_http_respond_html(vbc_socket_t client_sock, vbc_stack_t* stack, int status_code, vbc_string_t html) {
    vbc_http_respond(client_sock, stack, status_code, NULL, "text/html", html);
}

void vbc_http_respond_json(vbc_socket_t client_sock, vbc_stack_t* stack, int status_code, vbc_string_t json) {
    vbc_http_respond(client_sock, stack, status_code, NULL, "application/json", json);
}

static void default_404_handler(vbc_stack_t* stack, vbc_http_request_t* req, vbc_socket_t client_sock) {
    (void)req;
    vbc_string_t msg = vbc_str_lit("404 Not Found");
    vbc_http_respond_text(client_sock, stack, 404, msg);
}

void vbc_http_server_start(vbc_http_server_t* server, int max_requests) {
    if (!server) return;
    server->listener = vbc_socket_create_tcp_server(server->port, 128);
    if (!server->listener.is_valid) {
        fprintf(stderr, "VBC Http Server error: Failed to bind port %d\n", server->port);
        return;
    }
    server->is_running = true;

    int handled = 0;
    while (server->is_running) {
        if (max_requests > 0 && handled >= max_requests) break;

        char client_ip[64] = {0};
        int client_port = 0;
        vbc_socket_t client = vbc_socket_accept(server->listener, client_ip, sizeof(client_ip), &client_port);
        if (!client.is_valid) continue;

        // Reset arena mark for O(1) reclamation per request
        size_t mark = vbc_stack_mark(&server->req_stack);

        char buf[8192];
        int64_t bytes = vbc_socket_recv(client, buf, sizeof(buf) - 1);
        if (bytes > 0) {
            buf[bytes] = '\0';
            const char* header_end = find_crlf_crlf(buf, bytes);
            if (header_end) {
                vbc_http_request_t req;
                memset(&req, 0, sizeof(req));
                req.headers = vbc_slice_create(&server->req_stack, 8, sizeof(vbc_http_header_t));
                req.remote_ip = vbc_str_lit(client_ip);
                req.remote_port = client_port;

                char method_buf[16] = {0};
                char path_buf[512] = {0};
                sscanf(buf, "%15s %511s", method_buf, path_buf);

                req.method = vbc_str_alloc(&server->req_stack, (int64_t)strlen(method_buf));
                memcpy((char*)req.method.data, method_buf, strlen(method_buf));

                char* query_q = strchr(path_buf, '?');
                if (query_q) {
                    *query_q = '\0';
                    req.query = vbc_str_lit(query_q + 1);
                } else {
                    req.query = vbc_str_empty();
                }

                req.path = vbc_str_alloc(&server->req_stack, (int64_t)strlen(path_buf));
                memcpy((char*)req.path.data, path_buf, strlen(path_buf));

                const char* first_crlf = strstr(buf, "\r\n");
                if (first_crlf && first_crlf < header_end) {
                    parse_headers(&server->req_stack, first_crlf + 2, header_end, &req.headers);
                }

                const char* body_start = header_end + 4;
                int64_t body_len = bytes - (body_start - buf);
                req.body = vbc_str_alloc(&server->req_stack, body_len);
                memcpy((char*)req.body.data, body_start, (size_t)body_len);

                // Dispatch to matching route
                bool matched = false;
                for (int i = 0; i < server->route_count; i++) {
                    if (strcmp(server->routes[i].path, path_buf) == 0 &&
                        (strcmp(server->routes[i].method, "*") == 0 ||
                         strcmp(server->routes[i].method, method_buf) == 0)) {
                        server->routes[i].handler(&server->req_stack, &req, client);
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    default_404_handler(&server->req_stack, &req, client);
                }
            }
        }

        vbc_socket_close(&client);
        // O(1) Goose reclamation of entire request data stack!
        vbc_stack_reset(&server->req_stack, mark);
        handled++;
    }
}

void vbc_http_server_stop(vbc_http_server_t* server) {
    if (server) {
        server->is_running = false;
        vbc_socket_close(&server->listener);
    }
}

void vbc_http_server_destroy(vbc_http_server_t* server) {
    if (server) {
        vbc_http_server_stop(server);
        vbc_stack_free(&server->req_stack);
        free(server);
    }
}
