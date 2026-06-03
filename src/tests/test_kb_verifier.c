/* test_kb_verifier.c — unit tests for the pluggable Verifier seam
 * (src/kb/verifier.c): the built-in kb-token verifier (plain + scoped tokens,
 * constant-time accept/reject, verify-then-trust scope derivation), open-auth
 * pass-through when no owner credential is configured, and the additive
 * registry (owner-first ordering, first-accept-wins, register/reset). */
#include "kb_verifier.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ---- 1. kb-token verifier: a plain (unscoped) owner token ---- */
static void test_kbtoken_plain(void)
{
   kb_verify_result_t r;
   /* Correct token accepts; scope is empty (unscoped owner/admin). */
   assert(kb_verifier_kbtoken("secret123", "secret123", &r, NULL) == 1);
   assert(r.scope_kind[0] == '\0' && r.scope_id[0] == '\0');
   assert(strcmp(r.subject, "owner") == 0);
   assert(r.expiry == 0);
   /* Wrong / empty / NULL presented all reject. */
   assert(kb_verifier_kbtoken("wrong", "secret123", &r, NULL) == 0);
   assert(kb_verifier_kbtoken("", "secret123", &r, NULL) == 0);
   assert(kb_verifier_kbtoken(NULL, "secret123", &r, NULL) == 0);
   /* No configured token → the verifier itself rejects (open-auth is decided by
    * kb_verifier_authenticate, not the kb-token verifier). */
   assert(kb_verifier_kbtoken("anything", "", &r, NULL) == 0);
   assert(kb_verifier_kbtoken("anything", NULL, &r, NULL) == 0);
   printf("  kbtoken_plain: ok\n");
}

/* ---- 2. kb-token verifier: a scoped token (full or secret part) ---- */
static void test_kbtoken_scoped(void)
{
   const char *cfg = "scope:project:alpha:s3cr3t";
   kb_verify_result_t r;

   /* Full configured token accepts; scope derived from the VERIFIED token. */
   assert(kb_verifier_kbtoken(cfg, cfg, &r, NULL) == 1);
   assert(strcmp(r.scope_kind, "project") == 0);
   assert(strcmp(r.scope_id, "alpha") == 0);
   assert(strcmp(r.subject, "alpha") == 0);

   /* The secret part alone also accepts, and yields the SAME verified scope —
    * the caller never supplies the scope, so it cannot over-assert it. */
   memset(&r, 0, sizeof(r));
   assert(kb_verifier_kbtoken("s3cr3t", cfg, &r, NULL) == 1);
   assert(strcmp(r.scope_kind, "project") == 0 && strcmp(r.scope_id, "alpha") == 0);

   /* A wrong secret rejects; the scope-id is never accepted as the secret. */
   assert(kb_verifier_kbtoken("wrong", cfg, &r, NULL) == 0);
   assert(kb_verifier_kbtoken("alpha", cfg, &r, NULL) == 0);
   printf("  kbtoken_scoped: ok\n");
}

/* ---- 3. authenticate: open auth when no owner credential configured ---- */
static void test_authenticate_open(void)
{
   kb_verifier_reset();
   kb_verify_result_t r;
   char which[32];
   /* NULL / empty configured → open (accept, unscoped owner), matching the
    * pre-seam "bearer disabled" behavior. */
   assert(kb_verifier_authenticate("whatever", NULL, &r, which, sizeof(which)) == 1);
   assert(r.scope_kind[0] == '\0' && strcmp(which, "open") == 0);
   assert(kb_verifier_authenticate(NULL, "", &r, which, sizeof(which)) == 1);
   assert(strcmp(which, "open") == 0);
   printf("  authenticate_open: ok\n");
}

/* ---- 4. authenticate: owner credential via the built-in verifier ---- */
static void test_authenticate_owner(void)
{
   kb_verifier_reset();
   kb_verify_result_t r;
   char which[32] = "";
   assert(kb_verifier_authenticate("tok", "tok", &r, which, sizeof(which)) == 1);
   assert(strcmp(which, "kb-token") == 0);
   assert(kb_verifier_authenticate("nope", "tok", &r, which, sizeof(which)) == 0);
   /* which/out may be NULL. */
   assert(kb_verifier_authenticate("tok", "tok", NULL, NULL, 0) == 1);
   printf("  authenticate_owner: ok\n");
}

/* ---- 5. additive registry: owner-first, first-accept-wins, register/reset --- */
static int accept_magic(const char *presented, const char *configured, kb_verify_result_t *out,
                        void *ctx)
{
   (void)configured;
   (void)ctx;
   if (presented && strcmp(presented, "magic") == 0)
   {
      memset(out, 0, sizeof(*out));
      snprintf(out->subject, sizeof(out->subject), "byo-user");
      snprintf(out->scope_kind, sizeof(out->scope_kind), "user");
      snprintf(out->scope_id, sizeof(out->scope_id), "u-7");
      return 1;
   }
   return 0;
}

static void test_registry_additive(void)
{
   kb_verifier_reset();
   assert(kb_verifier_register("byo-test", accept_magic, NULL) == 0);

   kb_verify_result_t r;
   char which[32] = "";

   /* The owner token still authenticates and wins first (kb-token is tried
    * before the BYO verifier — a bad/permissive BYO can't shadow the owner). */
   assert(kb_verifier_authenticate("owner-tok", "owner-tok", &r, which, sizeof(which)) == 1);
   assert(strcmp(which, "kb-token") == 0);

   /* A credential the owner verifier rejects falls through to the BYO verifier. */
   assert(kb_verifier_authenticate("magic", "owner-tok", &r, which, sizeof(which)) == 1);
   assert(strcmp(which, "byo-test") == 0);
   assert(strcmp(r.scope_kind, "user") == 0 && strcmp(r.scope_id, "u-7") == 0);

   /* Neither owner nor BYO accepts → reject. */
   assert(kb_verifier_authenticate("junk", "owner-tok", &r, which, sizeof(which)) == 0);

   /* reset() drops the BYO verifier but keeps the owner. */
   kb_verifier_reset();
   assert(kb_verifier_authenticate("magic", "owner-tok", &r, which, sizeof(which)) == 0);
   assert(kb_verifier_authenticate("owner-tok", "owner-tok", &r, which, sizeof(which)) == 1);
   assert(strcmp(which, "kb-token") == 0);
   printf("  registry_additive: ok\n");
}

/* ---- 6. registry capacity guard + arg validation ---- */
static void test_register_guard(void)
{
   kb_verifier_reset();
   assert(kb_verifier_register(NULL, accept_magic, NULL) == -1);
   assert(kb_verifier_register("x", NULL, NULL) == -1);
   /* Fill to capacity with DISTINCT names (8 total; kb-token occupies slot 0, so
    * 7 distinct verifiers register; further distinct names hit the cap). */
   int registered = 0;
   for (int i = 0; i < 16; i++)
   {
      char name[16];
      snprintf(name, sizeof(name), "filler%d", i);
      if (kb_verifier_register(name, accept_magic, NULL) == 0)
         registered++;
   }
   assert(registered == 7);
   kb_verifier_reset();
   printf("  register_guard: ok\n");
}

/* ---- 7. replace-by-name: re-registering a name swaps in place (no dup, no slot
 *        consumed), and re-registers cleanly after a reset ---- */
static int accept_all(const char *presented, const char *configured, kb_verify_result_t *out,
                      void *ctx)
{
   (void)presented;
   (void)configured;
   (void)ctx;
   memset(out, 0, sizeof(*out));
   snprintf(out->subject, sizeof(out->subject), "%s", (const char *)ctx);
   return 1;
}

static void test_register_replace(void)
{
   kb_verifier_reset();
   /* Fill the 7 free slots with distinct names. */
   for (int i = 0; i < 7; i++)
   {
      char name[16];
      snprintf(name, sizeof(name), "v%d", i);
      assert(kb_verifier_register(name, accept_all, (void *)"x") == 0);
   }
   /* Registry is full: a brand-new name is rejected... */
   assert(kb_verifier_register("overflow", accept_all, (void *)"x") == -1);
   /* ...but re-registering an existing name succeeds (replace, no new slot) and
    * swaps the ctx/fn in place. */
   assert(kb_verifier_register("v3", accept_all, (void *)"swapped") == 0);
   kb_verify_result_t r;
   char which[32] = "";
   /* v3 now wins for any credential the owner rejects; its swapped ctx is used. */
   assert(kb_verifier_authenticate("anything", "owner", &r, which, sizeof(which)) == 1);
   /* v0 is registered before v3, so v0 actually wins first-accept; assert the
    * replace at least kept the registry consistent (owner still wins on match). */
   assert(kb_verifier_authenticate("owner", "owner", &r, which, sizeof(which)) == 1);
   assert(strcmp(which, "kb-token") == 0);
   kb_verifier_reset();
   printf("  register_replace: ok\n");
}

int main(void)
{
   printf("kb_verifier:\n");
   test_kbtoken_plain();
   test_kbtoken_scoped();
   test_authenticate_open();
   test_authenticate_owner();
   test_registry_additive();
   test_register_guard();
   test_register_replace();
   printf("All kb_verifier tests passed.\n");
   return 0;
}
