#ifndef NEHANDA_UNIFIED_STORAGE_H
#define NEHANDA_UNIFIED_STORAGE_H

#include <sqlite3.h>

sqlite3 *initialize_unified_storage(void);
int nehanda_apply_unified_schema(sqlite3 *db);
int verify_session_state_direct(sqlite3 *db, const char *session_id, char **session_active_out);

#endif
