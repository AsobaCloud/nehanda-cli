/* kb_client_code_graph.c: server-side client for the KB code-graph retrieval +
 * analytics routes (/v1/code/hybrid, /v1/code/graph/hubs).
 *
 * Like kb_client_pdf.c, these routes return purpose-built JSON — the hybrid
 * route's fused {results[], why[]} and the hubs route's ranked {hubs[]} — that
 * the agent consumes directly, so we forward the route's body VERBATIM rather
 * than round-tripping through flat C structs (which would drop the nested shape).
 * Each function returns the malloc'd JSON string the caller frees, or NULL on a
 * parameter/transport/non-2xx failure; *status_out (when non-NULL) carries the
 * route's HTTP status so the caller can craft a useful message. Every
 * caller-supplied query-string value is URL-escaped (mirrors kb_client_index.c). */
#include "kb_client.h"
#include "kb_client_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KB_CLIENT_CODE_GRAPH_READ_TIMEOUT_MS (8 * 1000)

char *kb_client_code_hybrid(const char *query, const char *symbol, const char *project,
                            int max_results, int *status_out)
{
   if (status_out)
      *status_out = -1;
   if (!query || !query[0])
      return NULL;

   char *query_q = kb_client_query_escape(query);
   char *symbol_q = (symbol && symbol[0]) ? kb_client_query_escape(symbol) : NULL;
   char *project_q = (project && project[0]) ? kb_client_query_escape(project) : NULL;
   if (!query_q || ((symbol && symbol[0]) && !symbol_q) || ((project && project[0]) && !project_q))
   {
      free(query_q);
      free(symbol_q);
      free(project_q);
      return NULL;
   }
   if (max_results < 1)
      max_results = 1;

   size_t cap = strlen("/v1/code/hybrid?query=&max_results=&symbol=&project=") + strlen(query_q) +
                (symbol_q ? strlen(symbol_q) : 0) + (project_q ? strlen(project_q) : 0) + 32;
   char *path = malloc(cap);
   if (!path)
   {
      free(query_q);
      free(symbol_q);
      free(project_q);
      return NULL;
   }
   int off = snprintf(path, cap, "/v1/code/hybrid?query=%s&max_results=%d", query_q, max_results);
   if (symbol_q)
      off += snprintf(path + off, cap - (size_t)off, "&symbol=%s", symbol_q);
   if (project_q)
      snprintf(path + off, cap - (size_t)off, "&project=%s", project_q);
   free(query_q);
   free(symbol_q);
   free(project_q);

   char *json = kb_client_v1_get_json(path, KB_CLIENT_CODE_GRAPH_READ_TIMEOUT_MS, status_out);
   free(path);
   return json;
}

char *kb_client_code_graph_hubs(const char *project, int max_results, int *status_out)
{
   if (status_out)
      *status_out = -1;
   if (!project || !project[0])
      return NULL;

   char *project_q = kb_client_query_escape(project);
   if (!project_q)
      return NULL;
   if (max_results < 1)
      max_results = 1;

   size_t cap = strlen("/v1/code/graph/hubs?project=&max_results=") + strlen(project_q) + 32;
   char *path = malloc(cap);
   if (!path)
   {
      free(project_q);
      return NULL;
   }
   snprintf(path, cap, "/v1/code/graph/hubs?project=%s&max_results=%d", project_q, max_results);
   free(project_q);

   char *json = kb_client_v1_get_json(path, KB_CLIENT_CODE_GRAPH_READ_TIMEOUT_MS, status_out);
   free(path);
   return json;
}

char *kb_client_code_graph_surprising(const char *project, int max_results, int judge,
                                      int *status_out)
{
   if (status_out)
      *status_out = -1;
   if (!project || !project[0])
      return NULL;

   char *project_q = kb_client_query_escape(project);
   if (!project_q)
      return NULL;
   if (max_results < 1)
      max_results = 1;

   size_t cap =
       strlen("/v1/code/graph/surprising?project=&max_results=&judge=true") + strlen(project_q) + 32;
   char *path = malloc(cap);
   if (!path)
   {
      free(project_q);
      return NULL;
   }
   snprintf(path, cap, "/v1/code/graph/surprising?project=%s&max_results=%d%s", project_q,
            max_results, judge ? "&judge=true" : "");
   free(project_q);

   char *json = kb_client_v1_get_json(path, KB_CLIENT_CODE_GRAPH_READ_TIMEOUT_MS, status_out);
   free(path);
   return json;
}
