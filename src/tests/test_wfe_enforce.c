/* test_wfe_enforce.c -- S2 pure enforcement cores: deliver-primitive set,
 * per-block tool surface, surface allow-check (with the delivery gate), and the
 * rollout fail-class split. */
#include <assert.h>
#include <stdio.h>

#include "wfe_enforce.h"
#include "wfe_externalization.h"
#include "wfe_iface.h"

int main(void)
{
   printf("wfe-enforce: ");

   /* --- DELIVER_PRIMITIVES set --- */
   assert(wfe_is_deliver_primitive("pr.open") && wfe_is_deliver_primitive("merge"));
   assert(wfe_is_deliver_primitive("deploy") && wfe_is_deliver_primitive("release"));
   assert(wfe_is_deliver_primitive("mcp__github__create_pull_request")); /* substring */
   assert(!wfe_is_deliver_primitive("Read") && !wfe_is_deliver_primitive("Edit"));
   assert(!wfe_is_deliver_primitive("") && !wfe_is_deliver_primitive(NULL));

   /* --- write / delegate tool classification --- */
   assert(wfe_is_write_tool("Edit") && wfe_is_write_tool("Write") && wfe_is_write_tool("apply_patch"));
   assert(!wfe_is_write_tool("Read") && !wfe_is_write_tool("Grep") && !wfe_is_write_tool(NULL));
   assert(wfe_is_delegate_tool("delegate") && wfe_is_delegate_tool("Task") &&
          wfe_is_delegate_tool("Subagent"));
   assert(!wfe_is_delegate_tool("Read") && !wfe_is_delegate_tool(NULL));

   /* --- per-block default surface --- */
   assert(wfe_block_default_surface(WFE_BLK_UNDERSTAND) == WFE_SURFACE_READONLY);
   assert(wfe_block_default_surface(WFE_BLK_REVIEW) == WFE_SURFACE_READONLY);
   assert(wfe_block_default_surface(WFE_BLK_GATE_DELIVER) == WFE_SURFACE_READONLY);
   assert(wfe_block_default_surface(WFE_BLK_IMPLEMENT) == WFE_SURFACE_DELEGATE);
   assert(wfe_block_default_surface(WFE_BLK_DOCUMENT) == WFE_SURFACE_DELEGATE);
   assert(wfe_block_default_surface(WFE_BLK_PR_OPEN) == WFE_SURFACE_FULL);

   /* --- surface allow-check --- */
   /* READONLY: reads ok; write/delegate denied; deliver gated on delivery */
   assert(wfe_surface_allows(WFE_SURFACE_READONLY, "Read", 0) == 1);
   assert(wfe_surface_allows(WFE_SURFACE_READONLY, "Edit", 0) == 0);
   assert(wfe_surface_allows(WFE_SURFACE_READONLY, "delegate", 0) == 0);
   assert(wfe_surface_allows(WFE_SURFACE_READONLY, "pr.open", 0) == 0);
   assert(wfe_surface_allows(WFE_SURFACE_READONLY, "pr.open", 1) == 1); /* delivered */
   assert(wfe_surface_allows(WFE_SURFACE_READONLY, NULL, 0) == 0);

   /* DELEGATE: delegate ok, direct write denied, deliver gated */
   assert(wfe_surface_allows(WFE_SURFACE_DELEGATE, "delegate", 0) == 1);
   assert(wfe_surface_allows(WFE_SURFACE_DELEGATE, "Read", 0) == 1);
   assert(wfe_surface_allows(WFE_SURFACE_DELEGATE, "Edit", 0) == 0);
   assert(wfe_surface_allows(WFE_SURFACE_DELEGATE, "merge", 0) == 0);

   /* FULL: write/delegate ok, but deliver STILL gated pre-delivery */
   assert(wfe_surface_allows(WFE_SURFACE_FULL, "Edit", 0) == 1);
   assert(wfe_surface_allows(WFE_SURFACE_FULL, "delegate", 0) == 1);
   assert(wfe_surface_allows(WFE_SURFACE_FULL, "pr.open", 0) == 0);
   assert(wfe_surface_allows(WFE_SURFACE_FULL, "pr.open", 1) == 1);

   /* --- rollout fail-class split --- */
   /* a policy denial always fails closed */
   assert(wfe_enforce_fail_action(WFE_FAIL_POLICY, 0, 0) == WFE_ACT_FAIL_CLOSED);
   assert(wfe_enforce_fail_action(WFE_FAIL_POLICY, 1, 1) == WFE_ACT_FAIL_CLOSED);
   /* instrumentation failure: chat fails open, but a deliver/write fails closed in hard */
   assert(wfe_enforce_fail_action(WFE_FAIL_INSTRUMENTATION, 1, 1) == WFE_ACT_FAIL_CLOSED);
   assert(wfe_enforce_fail_action(WFE_FAIL_INSTRUMENTATION, 1, 0) == WFE_ACT_FAIL_OPEN_CHAT);
   assert(wfe_enforce_fail_action(WFE_FAIL_INSTRUMENTATION, 0, 1) == WFE_ACT_FAIL_OPEN_CHAT);

   printf("ok\n");
   return 0;
}
