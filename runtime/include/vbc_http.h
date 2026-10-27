#ifndef VBC_HTTP_H
#define VBC_HTTP_H

#include "vbc_runtime.h"
#include "vbc_socket.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    vbc_string_t name;
    vbc_string_t value;
} vbc_http_header_t;

typedef struct {
    vbc_string_t method;
    vbc_string_t path;
    vbc_string_t query;
    vbc_string_t body;
    vbc_slice_t headers; // elements are vbc_http_header_t
    vbc_string_t remote_ip;
    int remote_port;
} vbc_http_request_t;

typedef struct {
    int status_code;
    vbc_string_t status_text;
    vbc_string_t body;
    vbc_slice_t headers; // elements are vbc_http_header_t
} vbc_http_response_t;

vbc_string_t vbc_http_get_header(const vbc_slice_t* headers, const char* name);

/* ========================================================================= */
/* HTTP Client API                                                           */
/* ========================================================================= */

vbc_http_response_t vbc_http_client_get(vbc_stack_t* stack, vbc_string_t url);
vbc_http_response_t vbc_http_client_post(vbc_stack_t* stack, vbc_string_t url, vbc_string_t body, vbc_string_t content_type);

struct vbc_task;
struct vbc_task* vbc_http_client_get_async(vbc_stack_t* stack, vbc_string_t url);
struct vbc_task* vbc_http_client_get_string_async(vbc_stack_t* stack, vbc_string_t url);
struct vbc_task* vbc_http_client_post_async(vbc_stack_t* stack, vbc_string_t url, vbc_string_t body, vbc_string_t content_type);

/* ========================================================================= */
/* HTTP Server API                                                           */
/* ========================================================================= */

typedef void (*vbc_http_handler_fn)(vbc_stack_t* req_stack, vbc_http_request_t* req, vbc_socket_t client_sock);

typedef struct {
    const char* method;
    const char* path;
    vbc_http_handler_fn handler;
} vbc_http_route_t;

#define VBC_MAX_ROUTES 64

typedef struct {
    int port;
    vbc_socket_t listener;
    bool is_running;
    int route_count;
    vbc_http_route_t routes[VBC_MAX_ROUTES];
    vbc_stack_t req_stack; // Arena for handling incoming requests
} vbc_http_server_t;

vbc_http_server_t* vbc_http_server_create(int port);
void vbc_http_server_on(vbc_http_server_t* server, const char* method, const char* path, vbc_http_handler_fn handler);
void vbc_http_server_start(vbc_http_server_t* server, int max_requests); // max_requests <= 0 runs forever
void vbc_http_server_stop(vbc_http_server_t* server);
void vbc_http_server_destroy(vbc_http_server_t* server);

/* Server Response Helpers */
void vbc_http_respond(vbc_socket_t client_sock, vbc_stack_t* stack, int status_code, const char* status_text, const char* content_type, vbc_string_t body);
void vbc_http_respond_text(vbc_socket_t client_sock, vbc_stack_t* stack, int status_code, vbc_string_t text);
void vbc_http_respond_html(vbc_socket_t client_sock, vbc_stack_t* stack, int status_code, vbc_string_t html);
void vbc_http_respond_json(vbc_socket_t client_sock, vbc_stack_t* stack, int status_code, vbc_string_t json);

#ifdef __cplusplus
}
#endif

#endif /* VBC_HTTP_H */
