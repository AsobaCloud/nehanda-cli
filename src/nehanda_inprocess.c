#include "nehanda_inprocess.h"
#include "nehanda_unified_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static sqlite3 *g_unified_db;

sqlite3 *nehanda_unified_db(void)
{
    if (!g_unified_db)
        g_unified_db = initialize_unified_storage();
    return g_unified_db;
}

int execute_canonical_index_sync(sqlite3 *db, const char *prompt_payload)
{
    if (!db || !prompt_payload)
        return -1;

    char doc_id[64];
    snprintf(doc_id, sizeof(doc_id), "turn-%ld", (long)time(NULL));

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT OR REPLACE INTO kb_docs (doc_id, raw_content, doc_hash) "
        "VALUES (?, ?, ?)";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    sqlite3_bind_text(stmt, 1, doc_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, prompt_payload, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, "indexed-in-process", -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int handle_incoming_tool_invocation(ToolTurnContext *context, sqlite3 *unified_db)
{
    if (!context || !unified_db)
        return -1;

    printf("DEBUG: Intercepted turn lifecycle inside local thread boundary. Session: %s\n",
           context->session_id ? context->session_id : "(none)");

    char *session_active = NULL;
    int session_valid = verify_session_state_direct(unified_db, context->session_id,
                                                    &session_active);
    if (session_valid != 0) {
        fprintf(stderr,
                "ERROR: Session authentication validation failed via local state query.\n");
        return -1;
    }
    free(session_active);

    printf("DEBUG: Forwarding directly to internal knowledge graph. Processing payload strings...\n");
    int curation_result = execute_canonical_index_sync(unified_db, context->prompt_payload);
    if (curation_result != 0) {
        fprintf(stderr, "ERROR: Structural knowledge indexing pipeline trace failure.\n");
        return curation_result;
    }

    printf("DEBUG: turn initialization pipeline successfully processed natively.\n");
    return 0;
}
