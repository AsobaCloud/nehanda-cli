/* kb_verifier.h: the pluggable Verifier seam for aimee-kb's auth.
 *
 * Phase 2 of the distributed-mode-auth proposal. A `Verifier` turns a presented
 * credential into a verified identity:
 *
 *     verify(credential) -> { subject, scope, expiry } | reject
 *
 * Verifiers are ADDITIVE and tried in registration order; the first to accept
 * wins. The built-in "kb-token" verifier (the owner credential — aimee-kb's own
 * v1 opaque, scope-describing bearer token) is always registered first, so a
 * later misconfigured BYO verifier (e.g. a bad JWKS) can never lock the owner
 * out. BYO JWT/OIDC verifiers are a later phase; this header defines the seam
 * and the default verifier only.
 *
 * Invariant — VERIFY-THEN-TRUST: the resulting scope is derived ONLY from the
 * cryptographically verified credential, never from a caller-asserted field.
 * That is the line between "secure" and "cross-tenant leak". */
#ifndef DEC_KB_VERIFIER_H
#define DEC_KB_VERIFIER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* A verified identity. Scope (kind/id) empty == unscoped owner (admin). */
   typedef struct
   {
      char subject[128];   /* verified principal (token id, or "owner") */
      char scope_kind[32]; /* "" = unscoped/global owner */
      char scope_id[128];
      long expiry; /* unix seconds; 0 = no expiry (owner/opaque token) */
   } kb_verify_result_t;

   /* A verifier examines `presented` (the credential the caller offered) and
    * either ACCEPTS — fills *out and returns 1 — or REJECTS and returns 0.
    * `configured` is aimee-kb's own bearer token (the owner credential); the
    * built-in kb-token verifier validates against it. BYO verifiers ignore
    * `configured` and use their own (e.g. JWKS) config carried in `ctx`. */
   typedef int (*kb_verifier_fn)(const char *presented, const char *configured,
                                 kb_verify_result_t *out, void *ctx);

   /* Register an additive verifier, tried after the built-in kb-token verifier.
    * Returns 0 on success, -1 if the registry is full or args are invalid. */
   int kb_verifier_register(const char *name, kb_verifier_fn fn, void *ctx);

   /* Reset the registry to just the built-in kb-token verifier (test helper /
    * reconfiguration). The owner verifier is never removed. */
   void kb_verifier_reset(void);

   /* Authenticate `presented` against `configured` by trying every registered
    * verifier in order (kb-token first). On the first acceptance: returns 1,
    * fills *out, and copies the winning verifier's name into which[which_cap]
    * (when which != NULL). If all reject: returns 0. When `configured` is NULL
    * or empty there is no owner credential configured and auth is open: returns
    * 1 with an empty (unscoped owner) result, matching the pre-seam behavior. */
   int kb_verifier_authenticate(const char *presented, const char *configured,
                                kb_verify_result_t *out, char *which, size_t which_cap);

   /* The built-in kb-token verifier, exposed for direct unit testing. Validates
    * the v1 opaque token: `presented` must equal `configured` in full, or — when
    * `configured` is a self-describing "scope:<kind>:<id>:<secret>" token — its
    * secret part. Scope (kind/id) is derived from the verified `configured`
    * token only (verify-then-trust). Comparison is constant-time. */
   int kb_verifier_kbtoken(const char *presented, const char *configured, kb_verify_result_t *out,
                           void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* DEC_KB_VERIFIER_H */
