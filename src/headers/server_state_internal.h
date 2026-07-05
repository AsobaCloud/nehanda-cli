#ifndef SERVER_STATE_INTERNAL_H
#define SERVER_STATE_INTERNAL_H
#include <stddef.h>
#include "cJSON.h"
#include "server.h"
/* Cross-TU declarations for the server_state cluster (server_state.c + the .c
 * files split out of it). Formerly file-local statics shared by textual .inc. */
/* promoted cross-TU (former .inc statics) */
int send_and_free(server_conn_t *conn, cJSON *resp);
int workspace_rpc_args(cJSON *req, char **argv, int max);
void ws_git_line(const char *const argv[], char *out, size_t outsz);

#endif /* SERVER_STATE_INTERNAL_H */
