/* server_seed_config.inc: seed the built-in personas and delegate role templates
 * into user config at server boot. Factored out of server.c to keep that TU
 * under the line cap. Included once, after server.c's headers (needs
 * config_default_dir / LOG_WARN / MAX_PATH_LEN). */
#include "persona.h"        /* persona_install_defaults */
#include "role_templates.h" /* role_template_install_defaults */

/* Seed the built-in personas and delegate role templates into user config as
 * editable files. This is what makes config — not code — the source of truth:
 * once seeded, every persona/role lookup reads the on-disk file (the built-in
 * prose in code only populates the default and is the fallback when a file is
 * missing). Idempotent: existing files are never overwritten, so user edits
 * survive restarts. */
static void server_seed_config_defaults(void)
{
   char pdir[MAX_PATH_LEN];
   snprintf(pdir, sizeof(pdir), "%s/personas", config_default_dir());
   if (persona_install_defaults(pdir) < 0)
      LOG_WARN("server", "could not seed default personas in %s", pdir);
   char rdir[MAX_PATH_LEN];
   snprintf(rdir, sizeof(rdir), "%s/role_templates", config_default_dir());
   if (role_template_install_defaults(rdir) < 0)
      LOG_WARN("server", "could not seed default role templates in %s", rdir);
}
