#ifndef DEC_MCP_TOOLS_H
#define DEC_MCP_TOOLS_H 1

#include "cJSON.h"

/* Build the complete MCP tools list (core + git tools).
 * Returns a cJSON array suitable for tools/list responses. */
cJSON *mcp_build_tools_list(void);

#endif /* DEC_MCP_TOOLS_H */
