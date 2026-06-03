#ifndef DEC_SERVER_MCP_DELEGATE_H
#define DEC_SERVER_MCP_DELEGATE_H 1

#include "server.h"

int handle_mcp_delegate_call(server_ctx_t *ctx, server_conn_t *conn, cJSON *args, const char *sid);

#endif /* DEC_SERVER_MCP_DELEGATE_H */
