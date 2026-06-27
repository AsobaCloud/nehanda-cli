/* server/harness_memory_routes.c — op handlers for the harness_memory.* family,
 * exposed at the /v1/harness_memory/ routes (see server_http_routes.inc). Thin
 * layer over the DB1 store (db1/harness_memory.h); server is the sole DB1 writer. */

#include "server.h"

#include "cJSON.h"
#include "db1/harness_memory.h"
#include "harness_memory_common.h"
#include "json_fluent.h"

#include <stdlib.h>
#include <string.h>

static int send_and_free(server_conn_t *conn, cJSON *resp)
{
   return server_send_ok(conn, resp);
}

static cJSON *hmem_row_json(const hmem_row_t *r)
{
   cJSON *o = cJSON_CreateObject();
   if (!o)
      return NULL;
   cJSON_AddNumberToObject(o, "id", (double)r->id);
   cJSON_AddStringToObject(o, "project", r->project);
   cJSON_AddStringToObject(o, "name", r->name);
   cJSON_AddStringToObject(o, "type", r->type);
   cJSON_AddStringToObject(o, "description", r->description ? r->description : "");
   cJSON_AddStringToObject(o, "body", r->body ? r->body : "");
   cJSON_AddStringToObject(o, "meta_json", r->meta_json ? r->meta_json : "{}");
   cJSON_AddStringToObject(o, "content_hash", r->content_hash);
   cJSON_AddStringToObject(o, "last_client", r->last_client);
   cJSON_AddStringToObject(o, "created_at", r->created_at);
   cJSON_AddStringToObject(o, "updated_at", r->updated_at);
   return o;
}

int handle_hmem_upsert(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   const char *project, *name;
   if (jo_need_str(req, "project", &project) < 0 || jo_need_str(req, "name", &name) < 0)
      return server_send_error(conn, "missing project or name", NULL);

   hmem_row_t in;
   memset(&in, 0, sizeof(in));
   snprintf(in.project, sizeof(in.project), "%s", project);
   snprintf(in.name, sizeof(in.name), "%s", name);
   snprintf(in.type, sizeof(in.type), "%s", jo_str(req, "type", "fact"));
   if (!hmem_type_valid(in.type))
      return server_send_error(conn, "invalid type", NULL);
   snprintf(in.last_client, sizeof(in.last_client), "%s", jo_str(req, "client", ""));
   snprintf(in.source_session, sizeof(in.source_session), "%s", jo_str(req, "session_id", ""));
   /* borrowed pointers — hmem_upsert only reads them */
   in.description = (char *)jo_str(req, "description", "");
   in.body = (char *)jo_str(req, "body", "");
   in.meta_json = (char *)jo_str(req, "meta_json", "{}");

   int64_t id = 0;
   int rc = hmem_upsert(&in, &id);
   cJSON *resp = cJSON_CreateObject();
   cJSON_AddStringToObject(resp, "status", rc == 0 ? "ok" : "error");
   if (rc == 0)
      cJSON_AddNumberToObject(resp, "id", (double)id);
   return send_and_free(conn, resp);
}

int handle_hmem_get(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   const char *project, *name;
   if (jo_need_str(req, "project", &project) < 0 || jo_need_str(req, "name", &name) < 0)
      return server_send_error(conn, "missing project or name", NULL);

   hmem_row_t row;
   cJSON *resp;
   if (hmem_get(project, name, &row) == 0)
   {
      resp = jo_ok();
      cJSON_AddItemToObject(resp, "memory", hmem_row_json(&row));
      hmem_row_free_fields(&row);
   }
   else
   {
      resp = jo_err("not found");
   }
   return send_and_free(conn, resp);
}

static int respond_rows(server_conn_t *conn, hmem_row_t *rows, int n)
{
   cJSON *resp = jo_ok();
   cJSON *arr = cJSON_CreateArray();
   for (int i = 0; i < n; i++)
      cJSON_AddItemToArray(arr, hmem_row_json(&rows[i]));
   cJSON_AddItemToObject(resp, "memories", arr);
   hmem_rows_free(rows, n);
   return send_and_free(conn, resp);
}

int handle_hmem_list(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   const char *project;
   if (jo_need_str(req, "project", &project) < 0)
      return server_send_error(conn, "missing project", NULL);
   int include_deleted = jo_int(req, "include_deleted", 0);
   hmem_row_t *rows = NULL;
   int n = 0;
   if (hmem_list(project, &rows, &n, include_deleted) != 0)
      return server_send_error(conn, "list failed", NULL);
   return respond_rows(conn, rows, n);
}

int handle_hmem_search(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   const char *project, *query;
   if (jo_need_str(req, "project", &project) < 0 || jo_need_str(req, "query", &query) < 0)
      return server_send_error(conn, "missing project or query", NULL);
   hmem_row_t *rows = NULL;
   int n = 0;
   if (hmem_search(project, query, &rows, &n) != 0)
      return server_send_error(conn, "search failed", NULL);
   return respond_rows(conn, rows, n);
}

int handle_hmem_tombstone(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   const char *project, *name;
   if (jo_need_str(req, "project", &project) < 0 || jo_need_str(req, "name", &name) < 0)
      return server_send_error(conn, "missing project or name", NULL);
   int rc = hmem_tombstone(project, name);
   cJSON *resp = cJSON_CreateObject();
   cJSON_AddStringToObject(resp, "status", rc == 0 ? "ok" : "error");
   return send_and_free(conn, resp);
}

int handle_hmem_tombstone_prefix(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   const char *project;
   if (jo_need_str(req, "project", &project) < 0)
      return server_send_error(conn, "missing project", NULL);
   const char *dir = jo_str(req, "dir", "");
   int rc = hmem_tombstone_prefix(project, dir);
   cJSON *resp = cJSON_CreateObject();
   cJSON_AddStringToObject(resp, "status", rc >= 0 ? "ok" : "error");
   if (rc >= 0)
      cJSON_AddNumberToObject(resp, "tombstoned", (double)rc);
   return send_and_free(conn, resp);
}
