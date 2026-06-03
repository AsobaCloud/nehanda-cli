/* kb_payload.h: DB2 domain helper that builds vector payloads for kb chunks.
 *
 * Reads kb_documents rows and assembles the JSON body that the pgvector
 * transport attaches to a vector upsert. Pure DB2 data; no pgvector
 * transport here.
 */
#ifndef DEC_DB2_KB_PAYLOAD_H
#define DEC_DB2_KB_PAYLOAD_H 1

#include <stddef.h>
#include <stdint.h>

#include "../headers/sketch.h"

#ifdef __cplusplus
extern "C"
{
#endif

   /* Heap-allocated payload JSON for the kb_documents row, or NULL on
    * missing / SQL error.  Caller frees. */
   char *db2_kb_build_document_payload(int64_t doc_id);

   /* (id, file_path, heading_path, line_start, line_end, content) row
    * from kb_documents.  Field sizes mirror the kb_result_t struct
    * the search path consumes. */
   typedef struct
   {
      int64_t id;
      char file_path[1024];
      char file_hash[32];
      char heading_path[256];
      int line_start;
      int line_end;
      char content[8192];
   } db2_kb_document_row_t;

   /* Look up a kb_documents row scoped by (id, project). Returns 1 on
    * hit (out filled), 0 on miss / SQL error / unavailable DB2. */
   int db2_kb_document_fetch(int64_t id, const char *project, db2_kb_document_row_t *out);

   /* Convention-candidate row from kb_documents — tier-friendly subset
    * of fields used by kb_extract_convention_candidates. */
   typedef struct
   {
      char project[64];
      char file_path[1024];
      char heading_path[256];
      char content[8192];
   } db2_kb_convention_row_t;

   /* Pull up to |max| chunks from kb_documents whose file_path matches
    * one of the convention-source patterns (CONTRIBUTING / AGENTS.md /
    * STYLE / CODING / .aimee-rules / .aimee/rules.md / .aimee/context.md
    * / /adr/). Ordered by project, file_path, chunk_index. Returns
    * rows written. */
   int db2_kb_documents_list_convention_candidates(db2_kb_convention_row_t *out, int max);

   /* Look up the stored file_hash for (project, file_path) in
    * kb_documents (any chunk row's column — they all share the same
    * file-level hash). Returns 0 on hit (out filled), -1 on miss /
    * SQL error / unavailable DB2. */
   int db2_kb_documents_get_stored_hash(const char *project, const char *file_path, char *out,
                                        size_t out_len);

   /* Return 1 when DB2 already has evidence or a file-index entry for
    * (project, file_hash), 0 when missing, -1 on SQL / connection error.
    * sample_path is optional and receives one matching path for diagnostics. */
   int db2_kb_documents_hash_exists(const char *project, const char *file_hash, char *sample_path,
                                    size_t sample_path_len);

   /* Build an HLL over distinct file_path values that share (project,
    * file_hash). Returns the number of distinct identifiers added, or
    * -1 on SQL / connection error. */
   int db2_kb_documents_hll_sources_for_hash(const char *project, const char *file_hash,
                                             sketch_hll_t *out);

   /* INSERT OR IGNORE a kb_async_jobs row: (kind, document_id, project,
    * status='pending'). Used by kb_build to enqueue an async embedding
    * job for a freshly-inserted chunk. Returns 0 on success, -1 on
    * SQL / connection error. */
   int db2_kb_async_enqueue(const char *kind, int64_t document_id, const char *project);

   /* Version-bump: re-enqueue extract_doc for every document. Returns count. */
   int db2_curator_reenqueue_extract_all(void);

   /* Invalidation: mark curator artifacts citing a (project,file_path)'s chunks
    * as stale. Call before the chunks are deleted. Returns the count. */
   int db2_curator_invalidate_doc(const char *project, const char *file_path);

   typedef struct
   {
      int64_t id;
      char source_kind[64];
      char source_id[256];
      int artifacts_stale;
      char created_at[32];
   } db2_curator_invalidation_t;

   /* Append an invalidation event (one per invalidated source). */
   void db2_curator_invalidation_record(const char *source_kind, const char *source_id,
                                        int artifacts_stale);
   /* Fetch invalidation events with id > since_id (ascending). Returns count. */
   int db2_curator_invalidations_since(int64_t since_id, db2_curator_invalidation_t *out, int max);

   /* List up to |max| kb_documents.id values that belong to
    * (project, file_path). Caller materializes the ids before issuing
    * follow-up writes (db3 point deletes + the bulk DELETE) because
    * libpq only supports one active result per connection. Returns
    * count written (0 on miss / no DB2). */
   int db2_kb_documents_list_chunk_ids_for_file(const char *project, const char *file_path,
                                                int64_t *out, int max);

   /* DELETE FROM kb_documents WHERE project = ? AND file_path = ?.
    * Best-effort; no return. Caller is responsible for any
    * accompanying db3 point deletes — those run on the materialized
    * id list returned by db2_kb_documents_list_chunk_ids_for_file. */
   void db2_kb_documents_delete_for_file(const char *project, const char *file_path);

   /* INSERT a fresh kb_documents row with DELETE-then-INSERT semantics
    * (callers invoke db2_kb_documents_delete_for_file for the file
    * before inserting any chunks). updated_at is stamped to now.
    * Returns the new document id, or -1 on SQL / connection error. */
   int64_t db2_kb_documents_insert_chunk(const char *project, const char *file_path,
                                         const char *file_hash, int chunk_index,
                                         const char *heading_path, int line_start, int line_end,
                                         const char *content, int token_count);

   /* Set prev_chunk_id on doc_id and next_chunk_id on prev_id, linking
    * a freshly-inserted chunk to its predecessor. No-op when prev_id
    * is non-positive. */
   void db2_kb_documents_link_neighbours(int64_t doc_id, int64_t prev_id);

#ifdef __cplusplus
}
#endif

#endif /* DEC_DB2_KB_PAYLOAD_H */
