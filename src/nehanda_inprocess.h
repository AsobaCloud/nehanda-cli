#ifndef NEHANDA_INPROCESS_H
#define NEHANDA_INPROCESS_H

#include <sqlite3.h>

typedef struct {
    const char *session_id;
    const char *prompt_payload;
    int token_count;
} ToolTurnContext;

int handle_incoming_tool_invocation(ToolTurnContext *context, sqlite3 *unified_db);
int execute_canonical_index_sync(sqlite3 *db, const char *prompt_payload);
sqlite3 *nehanda_unified_db(void);

#endif
