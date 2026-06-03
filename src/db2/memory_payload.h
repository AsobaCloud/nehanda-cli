/* memory_payload.h: DB2 domain helpers that build vector point payloads.
 *
 * The payload JSON is pure DB2 data — memories, memory_units,
 * memory_scopes, memory_entities rows assembled for the point body.
 * The pgvector upsert path calls these builders immediately before
 * writing.
 *
 * Pure domain API.  Backend access stays private to src/db2/.
 */
#ifndef DEC_DB2_MEMORY_PAYLOAD_H
#define DEC_DB2_MEMORY_PAYLOAD_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Build the vector payload JSON for a memory point. Returns a
    * heap-allocated string the caller must free(), or NULL on error
    * (missing row, SQL failure). */
   char *db2_memory_build_memory_payload(int64_t memory_id);

   /* Build the vector payload JSON for a memory_unit point. Same
    * ownership contract.  `memory_id_out` is filled with the parent
    * memory_id when non-NULL and the build succeeds. */
   char *db2_memory_build_unit_payload(int64_t unit_id, int64_t *memory_id_out);

   /* Fetch just the key + content fields for a memory row; used to build
    * the embed text in memory_embed().  0 on hit, -1 on miss. */
   int db2_memory_get_key_content(int64_t memory_id, char *key_out, int key_len, char *content_out,
                                  int content_len);

   /* Returns 1 if any memories row matches `key` exactly, 0 if not, -1
    * on error. Cheap exact-match probe used by trace mining etc. */
   int db2_memory_key_exists(const char *key);

   /* Total row count in the memories table. Returns 0 on error. */
   int64_t db2_memory_count(void);

#ifdef __cplusplus
}
#endif

#endif /* DEC_DB2_MEMORY_PAYLOAD_H */
