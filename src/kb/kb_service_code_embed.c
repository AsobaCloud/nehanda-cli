/* src/kb/kb_service_code_embed.c: KB-side code embedding refresh.
 *
 * All writes to code_embeddings run through this KB-side module; server and
 * CLI code must request refreshes via KB RPC and must not access DB2/pgvector
 * directly. */

#include "kb_service_code_embed.h"
#include "db2/code_index_ops.h"
#include "../db2/db2_internal.h"
#include "../db2/db_postgres.h"
#include "../db2/entity_nodes.h"
#include "../db2/pgvec_kb_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CE_ERRBUF               256
#define CE_TEXT_CAP             4096
#define CE_FALLBACK_MAX_CALLS   5
#define CE_FALLBACK_MAX_IMPORTS 5

int kb_code_embed_build_fallback_text(const char *project, const char *file_path, int64_t file_id,
                                      char *out, size_t cap)
{
   if (!project || !file_path || !out || cap == 0)
      return -1;
   memset(out, 0, cap);

   /* Start with file path. */
   int written = snprintf(out, cap, "file:%s:%s", project, file_path);
   if (written < 0 || (size_t)written >= cap)
      return written;

   if (file_id <= 0)
      return written;

   void *conn = db2_conn();
   if (!conn)
      return written;
   char err[CE_ERRBUF] = "";

   /* Append up to 5 definitions. */
   {
      static const char *sql = "SELECT name FROM terms WHERE file_id = ?1 AND kind IN"
                               " ('definition','function','method','class','struct') LIMIT 5";
      aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
      if (st)
      {
         aimee_pg_bind_int64(st, "?1", file_id);
         while (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW)
         {
            const char *name = aimee_pg_column_text(st, 0);
            if (name && (size_t)written < cap - 2)
            {
               int r = snprintf(out + written, cap - (size_t)written, " %s", name);
               if (r > 0)
                  written += r;
            }
         }
         aimee_pg_finalize(st);
      }
   }

   /* Append up to CE_FALLBACK_MAX_CALLS outgoing call names. */
   {
      static const char *sql = "SELECT callee FROM code_calls WHERE file_id = ?1"
                               " AND callee != '' ORDER BY line LIMIT 5";
      aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
      if (st)
      {
         aimee_pg_bind_int64(st, "?1", file_id);
         int calls = 0;
         while (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW &&
                calls < CE_FALLBACK_MAX_CALLS)
         {
            const char *name = aimee_pg_column_text(st, 0);
            if (name && (size_t)written < cap - 2)
            {
               int r = snprintf(out + written, cap - (size_t)written, " calls:%s", name);
               if (r > 0)
                  written += r;
               calls++;
            }
         }
         aimee_pg_finalize(st);
      }
   }

   /* Append up to CE_FALLBACK_MAX_IMPORTS import names. */
   {
      static const char *sql = "SELECT name FROM file_imports WHERE file_id = ?1 LIMIT 5";
      aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
      if (st)
      {
         aimee_pg_bind_int64(st, "?1", file_id);
         int imports = 0;
         while (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW &&
                imports < CE_FALLBACK_MAX_IMPORTS)
         {
            const char *name = aimee_pg_column_text(st, 0);
            if (name && (size_t)written < cap - 2)
            {
               int r = snprintf(out + written, cap - (size_t)written, " import:%s", name);
               if (r > 0)
                  written += r;
               imports++;
            }
         }
         aimee_pg_finalize(st);
      }
   }

   return written;
}

int kb_code_embed_refresh(const char *project, const char *scope, const char **paths,
                          int path_count, int batch_size, int max_points, int dry_run,
                          kb_code_embed_result_t *out)
{
   if (!project || !*project || !out)
      return -1;

   memset(out, 0, sizeof(*out));
   snprintf(out->project, sizeof(out->project), "%s", project);
   snprintf(out->scope, sizeof(out->scope), "%s", scope ? scope : "changed_files");
   out->dry_run = dry_run;
   snprintf(out->writer, sizeof(out->writer), "kb_service");

   int effective_batch = (batch_size > 0) ? batch_size : 128;
   int effective_max = (max_points > 0) ? max_points : 5000;

   void *conn = db2_conn();
   if (!conn)
   {
      /* No DB available — accept as a no-op (e.g., test env). */
      out->accepted = 1;
      snprintf(out->job_id, sizeof(out->job_id), "code-embeddings:%s:0", project);
      return 0;
   }
   char err[CE_ERRBUF] = "";

   /* Get project id. */
   static const char *proj_sql = "SELECT id FROM projects WHERE name = ?1 LIMIT 1";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, proj_sql, err, sizeof(err));
   if (!st)
      return -1;
   aimee_pg_bind_text(st, "?1", project);
   int64_t proj_id = -1;
   if (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW)
      proj_id = aimee_pg_column_int64(st, 0);
   aimee_pg_finalize(st);
   if (proj_id < 0)
   {
      /* Project not indexed yet — accept job immediately (no rows to embed). */
      out->accepted = 1;
      snprintf(out->job_id, sizeof(out->job_id), "code-embeddings:%s:0", project);
      return 0;
   }

   /* Collect files to embed. */
   static const char *files_sql = "SELECT id, path, hash FROM files WHERE project_id = ?1";
   st = aimee_pg_prepare(conn, files_sql, err, sizeof(err));
   if (!st)
      return -1;
   aimee_pg_bind_int64(st, "?1", proj_id);

   typedef struct
   {
      int64_t id;
      char path[512];
      char hash[128];
   } file_row_t;
   file_row_t *rows = NULL;
   int row_count = 0;
   int row_cap = 256;
   rows = (file_row_t *)malloc((size_t)row_cap * sizeof(file_row_t));
   if (!rows)
   {
      aimee_pg_finalize(st);
      return -1;
   }
   while (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW)
   {
      /* Apply path filter if provided. */
      const char *p = aimee_pg_column_text(st, 1);
      if (path_count > 0 && paths)
      {
         int match = 0;
         for (int i = 0; i < path_count; i++)
            if (paths[i] && p && strcmp(paths[i], p) == 0)
            {
               match = 1;
               break;
            }
         if (!match)
            continue;
      }
      if (row_count >= effective_max)
         break;
      if (row_count >= row_cap)
      {
         row_cap *= 2;
         file_row_t *tmp = (file_row_t *)realloc(rows, (size_t)row_cap * sizeof(file_row_t));
         if (!tmp)
            break;
         rows = tmp;
      }
      rows[row_count].id = aimee_pg_column_int64(st, 0);
      const char *path = aimee_pg_column_text(st, 1);
      const char *hash = aimee_pg_column_text(st, 2);
      snprintf(rows[row_count].path, sizeof(rows[row_count].path), "%s", path ? path : "");
      snprintf(rows[row_count].hash, sizeof(rows[row_count].hash), "%s", hash ? hash : "");
      row_count++;
   }
   aimee_pg_finalize(st);
   out->estimated_points = row_count;

   if (dry_run)
   {
      free(rows);
      out->accepted = 1;
      snprintf(out->job_id, sizeof(out->job_id), "code-embeddings:%s:%d", project, row_count);
      return 0;
   }

   /* Ensure code collection ready (dim=384 to match memory_embeddings). */
   pgvec_kb_service_ensure_code_collection(384);

   int embedded = 0;
   int skipped = 0;
   int batch_num = 0;
   (void)effective_batch; /* batch processing implemented in caller for now */

   for (int i = 0; i < row_count && embedded < effective_max; i++)
   {
      char node_key[GRAPH_ENDPOINT_MAX];
      if (db2_entity_node_key_file(project, rows[i].path, node_key, sizeof(node_key)) != 0)
         continue;

      /* Skip unchanged: check content_hash. */
      if (rows[i].hash[0] && pgvec_kb_service_code_exists_by_hash(project, node_key, rows[i].hash))
      {
         skipped++;
         continue;
      }

      /* Build deterministic fallback text (no LLM required). */
      char text[CE_TEXT_CAP];
      kb_code_embed_build_fallback_text(project, rows[i].path, rows[i].id, text, sizeof(text));

      /* files.id is a global IDENTITY primary key — already unique across every
       * project — so it is the point id directly. The previous
       * `rows[i].id + proj_id * 1000000` offset could collide once file ids
       * exceeded ~1M (e.g. id 1,500,000 in one project and id 500,000 in
       * another both map to point 2,500,000), silently overwriting another
       * project's code vector and code_index_ops row. */
      int64_t point_id = rows[i].id;

      /* Build payload JSON. node_key (<=512) + file_path (<=512) +
       * content_hash (<=128) + project can exceed a 1 KiB buffer; size it
       * generously and guard truncation so a malformed JSON payload is never
       * upserted into the pgvector point. */
      char payload[2048];
      int plen = snprintf(payload, sizeof(payload),
                          "{\"project\":\"%s\",\"node_key\":\"%s\",\"file_path\":\"%s\","
                          "\"content_hash\":\"%s\"}",
                          project, node_key, rows[i].path, rows[i].hash);
      if (plen < 0 || (size_t)plen >= sizeof(payload))
      {
         db2_code_index_op_record(point_id, project, node_key, rows[i].path, 0,
                                  "code payload too large to encode");
         continue;
      }

      /* Embed using deterministic fallback (text-to-float via simple hash).
       * Production embedding uses kb_service embedding pipeline; this fallback
       * gives a stable non-zero vector from text tokens for testing. */
      float vec[384];
      memset(vec, 0, sizeof(vec));
      {
         /* Simple deterministic hash embedding: project text chars into dims. */
         const unsigned char *tp = (const unsigned char *)text;
         for (int c = 0; *tp && c < 65536; c++, tp++)
            vec[*tp % 384] += 1.0f / 256.0f;
         /* Normalise. */
         float len = 0.0f;
         for (int d = 0; d < 384; d++)
            len += vec[d] * vec[d];
         if (len > 0.0f)
         {
            len = 1.0f / __builtin_sqrtf(len);
            for (int d = 0; d < 384; d++)
               vec[d] *= len;
         }
      }

      int up = pgvec_kb_service_code_upsert(point_id, vec, 384, project, node_key, rows[i].path, "",
                                            rows[i].hash, payload);
      /* Per-chunk replay bookkeeping so a failed embed is retried by
       * `memory repair --reset-stuck`, not orphaned. */
      db2_code_index_op_record(point_id, project, node_key, rows[i].path, up == 0,
                               up == 0 ? NULL : "code vector upsert failed");
      if (up == 0)
      {
         embedded++;
         batch_num++;
      }
   }

   free(rows);
   out->embedded = embedded;
   out->skipped_unchanged = skipped;
   out->accepted = 1;
   snprintf(out->job_id, sizeof(out->job_id), "code-embeddings:%s:%d", project, embedded);
   return 0;
}
