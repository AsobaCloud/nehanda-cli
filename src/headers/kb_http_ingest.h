/* kb_http_ingest.h: aimee-kb ingest API HTTP handlers.
 *
 * See docs/proposals/pending/aimee-kb-ingest-api-and-corpus-staging.md */
#pragma once

/* POST /v1/docs — body is raw multipart/form-data bytes.
 * Boundary is extracted from the first line of body (starts with --).
 * Returns HTTP status code, writes JSON into out_buf. */
int handle_post_docs(const char *body, int body_len, char *out_buf, int out_cap);

/* POST /v1/docs/manifest — JSON body:
 * {"scope":"global","docs":[{"doc_key":"path","content_hash":"..."}]}
 * Returns the subset of docs whose content_hash is not present for scope. */
int handle_post_docs_manifest(const char *body, int body_len, char *out_buf, int out_cap);

/* GET /v1/docs/{id} — doc_id is the string path segment. */
int handle_get_doc(const char *doc_id, char *out_buf, int out_cap);

/* DELETE /v1/docs/{id} */
int handle_delete_doc(const char *doc_id, char *out_buf, int out_cap);

/* GET /v1/review — query_string may contain cursor=<id> and limit=<n> */
int handle_get_review(const char *query_string, char *out_buf, int out_cap);
