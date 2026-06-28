/* Unit test for normalize_roundtable_brief: a large caller brief (a whole diff /
 * design doc / test plan) must NOT be silently truncated — the old 4096-byte cap
 * truncated reviewers' input mid-document. Verifies the raised, heap-allocated
 * bound and that the abuse backstop still caps. */

#include "client_constants.h" /* MAX_PATH_LEN (needed by agent_types.h) */
#include "cJSON.h"
#include "delegate_ensemble.h" /* roundtable_result_t, ROUNDTABLE_MAX_QUESTIONS */

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server/server_compute_roundtable.inc" /* unit under test (static fns) */

static cJSON *req_with_brief(const char *s)
{
   cJSON *r = cJSON_CreateObject();
   cJSON_AddStringToObject(r, "brief", s);
   return r;
}

int main(void)
{
   char err[128];

   /* small brief → not truncated, content preserved */
   {
      cJSON *r = req_with_brief("review this change carefully");
      normalized_roundtable_brief_t o = {0};
      assert(normalize_roundtable_brief(r, &o, err, sizeof(err)) == 0);
      assert(o.rendered && strstr(o.rendered, "review this change carefully") && o.truncated == 0);
      free(o.rendered);
      cJSON_Delete(r);
   }

   /* large brief (100 KB, e.g. a diff) → NOT truncated (regression: old 4096 cap) */
   {
      size_t n = 100 * 1024;
      char *big = malloc(n + 1);
      assert(big);
      memset(big, 'x', n);
      big[n] = '\0';
      cJSON *r = req_with_brief(big);
      normalized_roundtable_brief_t o = {0};
      assert(normalize_roundtable_brief(r, &o, err, sizeof(err)) == 0);
      assert(o.rendered && o.truncated == 0);
      assert(strlen(o.rendered) > n);    /* whole brief present + "focus:" framing */
      assert(strlen(o.rendered) > 4096); /* the old cap is gone */
      free(o.rendered);
      cJSON_Delete(r);
      free(big);
   }

   /* over the 256 KB abuse backstop → truncated flag set, never overflows */
   {
      size_t n = 300 * 1024;
      char *huge = malloc(n + 1);
      assert(huge);
      memset(huge, 'y', n);
      huge[n] = '\0';
      cJSON *r = req_with_brief(huge);
      normalized_roundtable_brief_t o = {0};
      assert(normalize_roundtable_brief(r, &o, err, sizeof(err)) == 0);
      assert(o.rendered && o.truncated == 1);
      assert(strlen(o.rendered) <= ROUNDTABLE_BRIEF_MAX_BYTES);
      free(o.rendered);
      cJSON_Delete(r);
      free(huge);
   }

   /* empty / whitespace-only brief → nothing rendered, no error */
   {
      cJSON *r = req_with_brief("   \t\n");
      normalized_roundtable_brief_t o = {0};
      assert(normalize_roundtable_brief(r, &o, err, sizeof(err)) == 0);
      assert(o.rendered == NULL);
      cJSON_Delete(r);
   }

   /* object brief (focus/fixes/invariants arrays) → arrays rendered, not truncated */
   {
      cJSON *r = cJSON_CreateObject();
      cJSON *b = cJSON_AddObjectToObject(r, "brief");
      cJSON *f = cJSON_AddArrayToObject(b, "focus");
      cJSON_AddItemToArray(f, cJSON_CreateString("check the lock ordering"));
      cJSON_AddItemToArray(f, cJSON_CreateString("verify fail-open"));
      normalized_roundtable_brief_t o = {0};
      assert(normalize_roundtable_brief(r, &o, err, sizeof(err)) == 0);
      assert(o.rendered && strstr(o.rendered, "check the lock ordering") &&
             strstr(o.rendered, "verify fail-open") && o.truncated == 0);
      free(o.rendered);
      cJSON_Delete(r);
   }

   /* object brief over the backstop → truncation is flagged, NOT a hard error,
    * and the rendered partial is still returned (no -1, no overflow) */
   {
      size_t n = 300 * 1024;
      char *huge = malloc(n + 1);
      assert(huge);
      memset(huge, 'z', n);
      huge[n] = '\0';
      cJSON *r = cJSON_CreateObject();
      cJSON *b = cJSON_AddObjectToObject(r, "brief");
      cJSON *f = cJSON_AddArrayToObject(b, "focus");
      cJSON_AddItemToArray(f, cJSON_CreateString(huge));
      normalized_roundtable_brief_t o = {0};
      assert(normalize_roundtable_brief(r, &o, err, sizeof(err)) == 0); /* not a hard error */
      assert(o.rendered && o.truncated == 1);
      assert(strlen(o.rendered) <= ROUNDTABLE_BRIEF_MAX_BYTES);
      free(o.rendered);
      cJSON_Delete(r);
      free(huge);
   }

   /* keep add_roundtable_arrays referenced (static-fn -Werror) — no-op on a
    * zeroed result, exercises the empty-arrays path. */
   {
      cJSON *resp = cJSON_CreateObject();
      roundtable_result_t result;
      memset(&result, 0, sizeof(result));
      add_roundtable_arrays(resp, &result);
      cJSON_Delete(resp);
   }

   printf("test_roundtable_brief: OK\n");
   return 0;
}
