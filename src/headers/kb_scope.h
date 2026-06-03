/* kb_scope.h: bearer-token scope parsing and per-request scope authorization
 * for the aimee-kb /v1 API.
 *
 * A configured bearer token may be self-describing:
 *   scope:<kind>:<id>:<secret>   — a scoped token (e.g. scope:project:foo:s3cr3t)
 *   <secret>                     — an unscoped/admin token (full access)
 *
 * Auth (does the presented secret match) is separate from authorization
 * (may a token scoped <kind>:<id> touch a resource scoped <req_kind>:<req_id>).
 * The decision logic here is pure so it is fully unit-testable without a live
 * server or DB.
 *
 * See docs/proposals/accepted/aimee-kb-service-and-public-api.md
 * (AC: "Bearer token with scope project:X cannot read or write artifacts at
 * workspace:Y"). */
#ifndef DEC_KB_SCOPE_H
#define DEC_KB_SCOPE_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Parse a configured bearer token into its scope (kind/id) and secret.
    * For "scope:<kind>:<id>:<secret>" the three parts are split out. For any
    * other string the whole token is the secret and kind/id are empty (admin).
    * Output buffers are always NUL-terminated. Returns 0 (always succeeds;
    * malformed scoped tokens degrade to admin so a typo never locks everyone
    * out — the secret still has to match). */
   int kb_scope_token_parse(const char *token, char *scope_kind, size_t kind_len, char *scope_id,
                            size_t id_len, char *secret, size_t secret_len);

   /* Authorization decision. A token scoped (token_kind:token_id) may access a
    * resource scoped (req_kind:req_id) iff:
    *   - the token is unscoped (token_kind empty) — admin, full access; or
    *   - token_kind == req_kind AND token_id == req_id (exact match).
    * Returns 1 if authorized, 0 if denied. */
   int kb_scope_authorized(const char *token_kind, const char *token_id, const char *req_kind,
                           const char *req_id);

   /* Extract the scope a request targets, from the query string
    * (scope=<kind>:<id>, project=<id>, or workspace=<id>) or the JSON body
    * ("scope_kind"+"scope_id", or "scope_user" → kind=user). query_string and
    * body may be NULL. Writes the resolved kind/id into the buffers. Returns 1
    * if a target scope was found, 0 if the request names no scope. */
   int kb_scope_request_target(const char *query_string, const char *body, char *kind,
                               size_t kind_len, char *id, size_t id_len);

#ifdef __cplusplus
}
#endif

#endif /* DEC_KB_SCOPE_H */
