/* test_sandbox.c: unit tests for sandbox config parsing, container detection,
 * and availability checks. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sandbox.h"

/* -------------------------------------------------------------------------
 * sandbox_mode_from_string / sandbox_mode_to_string
 * ---------------------------------------------------------------------- */

static void test_mode_from_string(void)
{
   assert(sandbox_mode_from_string("off") == SANDBOX_MODE_OFF);
   assert(sandbox_mode_from_string("workspace_only") == SANDBOX_MODE_WORKSPACE_ONLY);
   assert(sandbox_mode_from_string("allowlist") == SANDBOX_MODE_ALLOWLIST);
   /* Unknown input falls back to off */
   assert(sandbox_mode_from_string("unknown") == SANDBOX_MODE_OFF);
   assert(sandbox_mode_from_string(NULL) == SANDBOX_MODE_OFF);
   assert(sandbox_mode_from_string("") == SANDBOX_MODE_OFF);
}

static void test_mode_to_string(void)
{
   assert(strcmp(sandbox_mode_to_string(SANDBOX_MODE_OFF), "off") == 0);
   assert(strcmp(sandbox_mode_to_string(SANDBOX_MODE_WORKSPACE_ONLY), "workspace_only") == 0);
   assert(strcmp(sandbox_mode_to_string(SANDBOX_MODE_ALLOWLIST), "allowlist") == 0);
}

static void test_mode_roundtrip(void)
{
   const char *modes[] = {"off", "workspace_only", "allowlist", NULL};
   for (int i = 0; modes[i]; i++)
   {
      sandbox_mode_t m = sandbox_mode_from_string(modes[i]);
      assert(strcmp(sandbox_mode_to_string(m), modes[i]) == 0);
   }
}

/* -------------------------------------------------------------------------
 * sandbox_detect_container
 * ---------------------------------------------------------------------- */

static void test_detect_container_returns_int(void)
{
   /* Just verify it returns a valid boolean (0 or 1) without crashing */
   int r = sandbox_detect_container();
   assert(r == 0 || r == 1);
}

/* -------------------------------------------------------------------------
 * sandbox_available
 * ---------------------------------------------------------------------- */

static void test_available_returns_int_with_reason(void)
{
   const char *reason = NULL;
   int avail = sandbox_available(&reason);
   assert(avail == 0 || avail == 1);
   /* When unavailable, reason must be set */
   if (!avail)
      assert(reason != NULL && reason[0] != '\0');
}

static void test_available_null_reason_ok(void)
{
   /* Passing NULL for reason should not crash */
   int avail = sandbox_available(NULL);
   assert(avail == 0 || avail == 1);
}

/* -------------------------------------------------------------------------
 * sandbox_config_t defaults
 * ---------------------------------------------------------------------- */

static void test_config_defaults(void)
{
   sandbox_config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   assert(cfg.mode == SANDBOX_MODE_OFF);
   assert(cfg.network_isolated == 0);
   assert(cfg.allow_path_count == 0);
}

/* -------------------------------------------------------------------------
 * main
 * ---------------------------------------------------------------------- */

int main(void)
{
   test_mode_from_string();
   test_mode_to_string();
   test_mode_roundtrip();
   test_detect_container_returns_int();
   test_available_returns_int_with_reason();
   test_available_null_reason_ok();
   test_config_defaults();

   printf("sandbox: all tests passed\n");
   return 0;
}
