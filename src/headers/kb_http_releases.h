/* kb_http_releases.h: aimee-kb review queue and release lifecycle HTTP handlers.
 *
 * See docs/proposals/pending/aimee-kb-ingest-api-and-corpus-staging.md */
#pragma once

/* POST /v1/review/{id}/accept — body may contain {"release_id": <n>} */
int handle_post_review_accept(const char *doc_id, const char *body, int body_len, char *out_buf,
                              int out_cap);

/* POST /v1/review/{id}/reject — body: {"reason": "..."} */
int handle_post_review_reject(const char *doc_id, const char *body, int body_len, char *out_buf,
                              int out_cap);

/* POST /v1/releases — body: {"name": "..."} */
int handle_post_releases(const char *body, int body_len, char *out_buf, int out_cap);

/* POST /v1/releases/{id}/promote */
int handle_post_promote(const char *release_id, char *out_buf, int out_cap);

/* POST /v1/releases/{id}/rollback — body may contain {"target_release_id": <n>} */
int handle_post_rollback(const char *release_id, const char *body, int body_len, char *out_buf,
                         int out_cap);

/* GET /v1/releases/active */
int handle_get_active_release(char *out_buf, int out_cap);
