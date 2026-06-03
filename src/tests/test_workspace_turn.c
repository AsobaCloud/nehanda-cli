/* test_workspace_turn.c: a turn whose cwd is inside a registered `detached`
 * workspace binds the active provider to a detached provider; shared workspaces
 * and unregistered cwds stay on the shared provider. Config-backed (the binder
 * reads the registered providers via config_load). */
#include "workspace_turn.h"
#include "workspace_provider.h"
#include "config.h"
#include "aimee_home.h"
#include "platform_path.h"
#include "platform_test_util.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
   /* Isolated temp HOME so config_save/load never touch the real config. */
   char tmpdir[512];
   snprintf(tmpdir, sizeof(tmpdir), "%s/aimee-test-wsturn-XXXXXX", platform_tmpdir());
   assert(platform_mkdtemp(tmpdir) != NULL);
   platform_setenv("HOME", tmpdir);
   platform_unsetenv("AIMEE_HOME");
   platform_setenv("AIMEE_NO_CACHE", "1");

   /* Register two workspaces: one detached, one shared (default). */
   config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   config_load(&cfg);
   cfg.workspace_count = 2;
   snprintf(cfg.workspaces[0], MAX_PATH_LEN, "/tmp/ws-detached");
   snprintf(cfg.workspace_providers[0], sizeof(cfg.workspace_providers[0]), "detached");
   snprintf(cfg.workspaces[1], MAX_PATH_LEN, "/tmp/ws-shared");
   cfg.workspace_providers[1][0] = '\0';
   assert(config_save(&cfg) == 0);

   const workspace_provider_t *shared = workspace_provider_shared();

   /* cwd inside the detached workspace -> binds a detached provider active */
   {
      assert(workspace_provider_active() == shared); /* default */
      int bound = workspace_turn_bind_active("/tmp/ws-detached/src/file.c");
      assert(bound == 1);
      const workspace_provider_t *active = workspace_provider_active();
      assert(active != shared && active->kind == WS_PROVIDER_DETACHED);
      workspace_turn_unbind_active();
      assert(workspace_provider_active() == shared); /* restored */
   }

   /* the workspace root itself also matches */
   {
      assert(workspace_turn_bind_active("/tmp/ws-detached") == 1);
      assert(workspace_provider_active()->kind == WS_PROVIDER_DETACHED);
      workspace_turn_unbind_active();
   }

   /* cwd inside a shared workspace -> stays on shared */
   {
      assert(workspace_turn_bind_active("/tmp/ws-shared/x") == 0);
      assert(workspace_provider_active() == shared);
      workspace_turn_unbind_active(); /* no-op */
      assert(workspace_provider_active() == shared);
   }

   /* unregistered cwd, and a prefix that isn't a path boundary -> shared */
   {
      assert(workspace_turn_bind_active("/tmp/elsewhere") == 0);
      assert(workspace_provider_active() == shared);
      assert(workspace_turn_bind_active("/tmp/ws-detached-other/x") == 0); /* not a boundary */
      assert(workspace_provider_active() == shared);
   }

   /* NULL / empty cwd -> shared */
   assert(workspace_turn_bind_active(NULL) == 0);
   assert(workspace_turn_bind_active("") == 0);
   assert(workspace_provider_active() == shared);

   /* AC #6 — foreign-cwd trust gate (pure decision). A remote peer (not
    * trusted-local) supplying a raw absolute path that did NOT bind a detached
    * provider is rejected; every other combination is allowed. */
   {
      /* remote + raw foreign path + no detached bind -> REJECT */
      assert(workspace_turn_reject_foreign_cwd(0, 0, "/home/someone/repo") == 1);
      /* co-located peer (trusted_local) -> allowed (real server path) */
      assert(workspace_turn_reject_foreign_cwd(0, 1, "/home/someone/repo") == 0);
      /* detached workspace bound -> allowed (acts on the client) */
      assert(workspace_turn_reject_foreign_cwd(1, 0, "/home/someone/repo") == 0);
      /* no cwd / non-absolute / empty -> nothing to reject */
      assert(workspace_turn_reject_foreign_cwd(0, 0, NULL) == 0);
      assert(workspace_turn_reject_foreign_cwd(0, 0, "") == 0);
      assert(workspace_turn_reject_foreign_cwd(0, 0, "relative/path") == 0);
      /* traversal path -> not bound, but not a hard reject either */
      assert(workspace_turn_reject_foreign_cwd(0, 0, "/a/../etc") == 0);
   }

   /* safe_exec_capture_env: the explicit child env is honored (this is the seam
    * the mirror git runner uses to inject the forge GH_TOKEN env), and a NULL env
    * inherits the parent's. */
   {
      const char *argv[] = {"/bin/sh", "-c", "printf %s \"$WS_TURN_ENV_PROBE\"", NULL};
      char *env[] = {(char *)"WS_TURN_ENV_PROBE=mirror-token-ok", NULL};
      char *out = NULL;
      int rc = safe_exec_capture_env(argv, env, &out, 256);
      assert(rc == 0 && out && strcmp(out, "mirror-token-ok") == 0);
      free(out);

      /* NULL env → inherit; the probe var is absent in the parent → empty. */
      platform_unsetenv("WS_TURN_ENV_PROBE");
      out = NULL;
      rc = safe_exec_capture_env(argv, NULL, &out, 256);
      assert(rc == 0 && out && out[0] == '\0');
      free(out);
   }

   printf("workspace_turn: all tests passed\n");
   return 0;
}
