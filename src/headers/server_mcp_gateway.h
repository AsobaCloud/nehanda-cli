/* server_mcp_gateway.h: dispatch gateway MCP tool calls (send_message, ask_user). */
#pragma once
typedef struct cJSON cJSON;
cJSON *mcp_gateway_tool_dispatch(const char *tool, cJSON *jargs);