#ifndef SERVER_MCP_PROCESS_H
#define SERVER_MCP_PROCESS_H 1

#include "cJSON.h"

cJSON *server_mcp_process_tool(const char *tool, cJSON *args);

#endif /* SERVER_MCP_PROCESS_H */
