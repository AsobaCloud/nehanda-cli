#ifndef DEC_SERVER_MCP_LEARNING_H
#define DEC_SERVER_MCP_LEARNING_H 1

#include "cJSON.h"

cJSON *server_mcp_tool_rules_propose(cJSON *args);
cJSON *server_mcp_tool_rules_list(void);
cJSON *server_mcp_tool_learning_propose(cJSON *args);
cJSON *server_mcp_tool_learning_review(cJSON *args);

#endif
