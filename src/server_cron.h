/* server_cron.h: cron job RPC handlers and scheduler dispatch. */
#pragma once

#include "cJSON.h"
#include "config.h"
#include "server.h"

int cron_run_config_job(const cron_job_t *job, cJSON **out_resp);

int handle_cron_list(server_ctx_t *ctx, server_conn_t *conn, cJSON *req);
int handle_cron_add(server_ctx_t *ctx, server_conn_t *conn, cJSON *req);
int handle_cron_show(server_ctx_t *ctx, server_conn_t *conn, cJSON *req);
int handle_cron_history(server_ctx_t *ctx, server_conn_t *conn, cJSON *req);
int handle_cron_run(server_ctx_t *ctx, server_conn_t *conn, cJSON *req);
int handle_cron_enable(server_ctx_t *ctx, server_conn_t *conn, cJSON *req);
int handle_cron_disable(server_ctx_t *ctx, server_conn_t *conn, cJSON *req);
int handle_cron_remove(server_ctx_t *ctx, server_conn_t *conn, cJSON *req);
