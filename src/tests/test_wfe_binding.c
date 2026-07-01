/* test_wfe_binding.c -- S2 session<->work-item binding (DB1): bind, idempotent
 * re-bind (monotonic stage), single-writer conflict, get, unbind + reclaim. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "db1.h"
#include "wfe_binding.h"

int main(void)
{
   printf("wfe-binding: ");
   assert(db1_init(":memory:") == 0);

   char wi[80] = "", st[16] = "";
   /* not bound yet */
   assert(db1_wfe_binding_get("sess1", wi, sizeof wi, st, sizeof st) == 0);

   /* bind sess1 -> wi_A (advisory) */
   assert(db1_wfe_bind("sess1", "wi_A", "advisory") == 0);
   assert(db1_wfe_binding_get("sess1", wi, sizeof wi, st, sizeof st) == 1);
   assert(strcmp(wi, "wi_A") == 0 && strcmp(st, "advisory") == 0);

   /* idempotent re-bind of the same session: succeeds, stage NOT downgraded/changed */
   assert(db1_wfe_bind("sess1", "wi_A", "hard") == 0);
   assert(db1_wfe_binding_get("sess1", wi, sizeof wi, st, sizeof st) == 1);
   assert(strcmp(st, "advisory") == 0); /* monotonic: stamped once */

   /* single-writer: a DIFFERENT session cannot bind wi_A */
   assert(db1_wfe_bind("sess2", "wi_A", "advisory") == -2);
   /* but sess2 can bind a different work-item */
   assert(db1_wfe_bind("sess2", "wi_B", "advisory") == 0);

   /* bad args fail closed */
   assert(db1_wfe_bind(NULL, "wi_C", "off") == -1);
   assert(db1_wfe_bind("sess3", "", "off") == -1);

   /* unbind sess1 releases wi_A; a new session can then reclaim it */
   assert(db1_wfe_unbind("sess1") == 0);
   assert(db1_wfe_binding_get("sess1", wi, sizeof wi, st, sizeof st) == 0);
   assert(db1_wfe_bind("sess3", "wi_A", "soft") == 0); /* wi_A free -> reclaim ok */

   printf("ok\n");
   return 0;
}
