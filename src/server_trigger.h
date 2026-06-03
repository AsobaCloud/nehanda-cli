#pragma once
#include "server.h"

int handle_trigger_fire(server_ctx_t *ctx, server_conn_t *conn, cJSON *req);
int handle_trigger_list(server_ctx_t *ctx, server_conn_t *conn, cJSON *req);
int handle_trigger_status(server_ctx_t *ctx, server_conn_t *conn, cJSON *req);
int handle_trigger_cancel(server_ctx_t *ctx, server_conn_t *conn, cJSON *req);
