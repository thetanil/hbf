// hbf_simple/main.c
// Entry point for minimal QuickJS HTTP server using CivetWeb

#include "http_bridge.h"
#include "qjs_runner.h"
#include "hbf_simple/server_js.h"
#include "internal/core/log.h"
#include "civetweb.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

static int running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

static int http_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *req_info = mg_get_request_info(conn);
    char body[8192] = {0};
    int body_len = mg_read(conn, body, sizeof(body) - 1);
    if (body_len < 0) body_len = 0;

    qjs_request req = {0};
    bridge_civetweb_to_qjs(req_info, body, (size_t)body_len, &req);

    qjs_response resp = {0};
    int rc = qjs_handle_request(&req, &resp);

    if (rc != 0) {
        mg_printf(conn, "HTTP/1.1 500 Internal Server Error\r\n");
        mg_printf(conn, "Content-Type: text/plain\r\n\r\n");
        mg_printf(conn, "JavaScript execution failed\n");
        return 500;
    }

    bridge_qjs_to_civetweb(conn, &resp);
    bridge_free_response(&resp);
    return resp.status_code;
}

int main(int argc, char **argv) {
    int port = 5309;
    // Parse --port argument
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--port", 6) == 0) {
            const char *val = NULL;
            if (argv[i][6] == '=') {
                val = argv[i] + 7;
            } else if (i + 1 < argc) {
                val = argv[i + 1];
                ++i;
            }
            if (val) {
                port = atoi(val);
            }
        }
    }
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    const char *options[] = {
        "listening_ports", port_str,
        NULL
    };
    hbf_log_set_level(HBF_LOG_INFO);
    signal(SIGINT, handle_sigint);

    if (qjs_init() != 0) {
        fprintf(stderr, "QuickJS init failed\n");
        return 1;
    }

    struct mg_context *ctx = mg_start(NULL, 0, options);
    if (!ctx) {
        fprintf(stderr, "CivetWeb start failed\n");
        qjs_cleanup();
        return 1;
    }
    mg_set_request_handler(ctx, "/", http_handler, NULL);
    mg_set_request_handler(ctx, "/qjshealth", http_handler, NULL);
    mg_set_request_handler(ctx, "/echo", http_handler, NULL);

    printf("hbf_simple running on port %d\n", port);
    while (running) {
        sleep(1);
    }
    mg_stop(ctx);
    qjs_cleanup();
    printf("hbf_simple stopped\n");
    return 0;
}
