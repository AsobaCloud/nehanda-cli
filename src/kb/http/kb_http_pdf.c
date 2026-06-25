/* kb_http_pdf.c: structured-PDF Phase 2 read route — search_chunks (citation retrieval).
 * Lexical (case-insensitive content match) over PDF chunks, returning line-level
 * {page_no, bbox, quote} citations from kb_doc_regions. The DB query always excludes
 * non-PDF rows and withholds quarantine_state='pending' (restricted) documents; the
 * caller's token scope is already enforced by kb_http_route_ex before dispatch. */
#include "kb_http_pdf.h"

#include "cJSON.h"
#include "db2/kb_payload.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PDF_MAX_CHUNKS 10
/* A chunk is bounded to KB_PDF_MAX_CHUNK_LINES (100) lines = 100 regions; 200 is headroom. */
#define PDF_MAX_REGIONS 200

/* URL-decoded value of query param `key` from `qs` (handles %XX and '+'). Returns 1 if
 * found. */
static int pdf_qparam(const char *qs, const char *key, char *out, int outsz)
{
   if (!qs || !key || !out || outsz <= 0)
      return 0;
   out[0] = '\0';
   int klen = (int)strlen(key);
   for (const char *p = qs; *p; p++)
   {
      if ((p == qs || p[-1] == '&') && strncmp(p, key, (size_t)klen) == 0 && p[klen] == '=')
      {
         p += klen + 1;
         int i = 0;
         while (*p && *p != '&' && i < outsz - 1)
         {
            if (*p == '%' && p[1] && p[2])
            {
               char hex[3] = {p[1], p[2], 0};
               out[i++] = (char)strtol(hex, NULL, 16);
               p += 3;
            }
            else if (*p == '+')
            {
               out[i++] = ' ';
               p++;
            }
            else
            {
               out[i++] = *p++;
            }
         }
         out[i] = '\0';
         return 1;
      }
   }
   return 0;
}

int handle_get_pdf_search_route(const char *method, const char *query_string, char *out_buf,
                                int out_cap)
{
   if (!method || strcmp(method, "GET") != 0)
   {
      snprintf(out_buf, (size_t)out_cap, "{\"error\":\"method not allowed\"}");
      return 405;
   }

   char query[512] = "", project[128] = "", maxs[16] = "";
   if (!pdf_qparam(query_string, "query", query, sizeof(query)) || !query[0])
   {
      snprintf(out_buf, (size_t)out_cap, "{\"error\":\"missing query parameter\"}");
      return 400;
   }
   pdf_qparam(query_string, "project", project, sizeof(project));
   int max = PDF_MAX_CHUNKS;
   if (pdf_qparam(query_string, "max_results", maxs, sizeof(maxs)))
   {
      max = atoi(maxs);
      if (max < 1)
         max = 1;
      if (max > PDF_MAX_CHUNKS)
         max = PDF_MAX_CHUNKS;
   }

   db2_kb_pdf_chunk_t *chunks = malloc((size_t)PDF_MAX_CHUNKS * sizeof(*chunks));
   db2_kb_pdf_region_t *regs = malloc((size_t)PDF_MAX_REGIONS * sizeof(*regs));
   if (!chunks || !regs)
   {
      free(chunks);
      free(regs);
      snprintf(out_buf, (size_t)out_cap, "{\"error\":\"oom\"}");
      return 500;
   }

   int n = db2_kb_pdf_search_chunks(project[0] ? project : NULL, query, max, chunks);

   cJSON *root = cJSON_CreateObject();
   cJSON *arr = cJSON_AddArrayToObject(root, "chunks");
   for (int i = 0; i < n; i++)
   {
      cJSON *c = cJSON_CreateObject();
      cJSON_AddNumberToObject(c, "chunk_id", (double)chunks[i].chunk_id);
      cJSON_AddStringToObject(c, "document_key", chunks[i].document_key);
      cJSON_AddNumberToObject(c, "page_start", chunks[i].page_start);
      cJSON_AddNumberToObject(c, "page_end", chunks[i].page_end);
      cJSON_AddStringToObject(c, "content", chunks[i].content);
      cJSON_AddStringToObject(c, "sensitivity_class", chunks[i].sensitivity_class);

      cJSON *cits = cJSON_AddArrayToObject(c, "citations");
      int rn = db2_kb_doc_regions_for_chunk(chunks[i].chunk_id, regs, PDF_MAX_REGIONS);
      for (int j = 0; j < rn; j++)
      {
         cJSON *cit = cJSON_CreateObject();
         cJSON_AddNumberToObject(cit, "page_no", regs[j].page_no);
         cJSON *bbox = cJSON_AddArrayToObject(cit, "bbox");
         cJSON_AddItemToArray(bbox, cJSON_CreateNumber(regs[j].x0));
         cJSON_AddItemToArray(bbox, cJSON_CreateNumber(regs[j].y0));
         cJSON_AddItemToArray(bbox, cJSON_CreateNumber(regs[j].x1));
         cJSON_AddItemToArray(bbox, cJSON_CreateNumber(regs[j].y1));
         cJSON_AddStringToObject(cit, "quote", regs[j].quote);
         cJSON_AddNumberToObject(cit, "line_index", regs[j].line_index);
         cJSON_AddStringToObject(cit, "content_type", regs[j].content_type);
         cJSON_AddItemToArray(cits, cit);
      }
      cJSON_AddItemToArray(arr, c);
   }
   cJSON_AddNumberToObject(root, "total", n);

   char *s = cJSON_PrintUnformatted(root);
   int status = 200;
   if (!s)
   {
      snprintf(out_buf, (size_t)out_cap, "{\"error\":\"oom\"}");
      status = 500;
   }
   else if (strlen(s) >= (size_t)out_cap)
   {
      /* Never return truncated (invalid) JSON: signal the caller to narrow the request. */
      snprintf(out_buf, (size_t)out_cap,
               "{\"error\":\"result too large; reduce max_results or narrow the "
               "query\",\"code\":\"result_too_large\"}");
      status = 413;
   }
   else
   {
      snprintf(out_buf, (size_t)out_cap, "%s", s);
   }
   free(s);
   cJSON_Delete(root);
   free(chunks);
   free(regs);
   return status;
}
