#include "aimee.h"
#include "commands.h"
#include "toolset.h"

static void toolset_usage(void)
{
   fprintf(stderr, "Usage: aimee toolset <subcommand> [name]\n\n"
                   "Subcommands:\n"
                   "  list              List named toolsets\n"
                   "  show <name>       Show a toolset definition\n"
                   "  resolve <name>    Print the resolved tool list\n");
}

void cmd_toolset(app_ctx_t *ctx, int argc, char **argv)
{
   (void)ctx;
   if (argc < 1)
   {
      toolset_usage();
      return;
   }

   toolset_registry_t registry;
   char err[TOOLSET_ERROR_MAX] = "";
   if (toolset_registry_load_effective(&registry, err, sizeof(err)) != 0)
   {
      fprintf(stderr, "toolset: %s\n", err[0] ? err : "failed to load toolsets");
      return;
   }

   if (strcmp(argv[0], "list") == 0)
   {
      for (int i = 0; i < registry.count; i++)
      {
         char resolved[TOOLSET_MAX_TOOLS][TOOLSET_TOOL_MAX];
         int n = toolset_resolve(&registry, registry.sets[i].name, resolved, TOOLSET_MAX_TOOLS, err,
                                 sizeof(err));
         fprintf(stdout, "%-16s %3d%s\n", registry.sets[i].name, n < 0 ? 0 : n,
                 registry.sets[i].builtin ? "  built-in" : "");
      }
      return;
   }

   if ((strcmp(argv[0], "show") == 0 || strcmp(argv[0], "resolve") == 0) && argc < 2)
   {
      fprintf(stderr, "Usage: aimee toolset %s <name>\n", argv[0]);
      return;
   }

   if (strcmp(argv[0], "show") == 0)
   {
      const toolset_def_t *def = toolset_registry_find(&registry, argv[1]);
      if (!def)
      {
         fprintf(stderr, "toolset: unknown toolset '%s'\n", argv[1]);
         return;
      }
      fprintf(stdout, "%s%s\n", def->name, def->builtin ? " (built-in)" : "");
      if (def->include_count > 0)
      {
         fprintf(stdout, "include:");
         for (int i = 0; i < def->include_count; i++)
            fprintf(stdout, " %s", def->include[i]);
         fputc('\n', stdout);
      }
      if (def->tool_count > 0)
      {
         fprintf(stdout, "tools:");
         for (int i = 0; i < def->tool_count; i++)
            fprintf(stdout, " %s", def->tools[i]);
         fputc('\n', stdout);
      }
      return;
   }

   if (strcmp(argv[0], "resolve") == 0)
   {
      char resolved[TOOLSET_MAX_TOOLS][TOOLSET_TOOL_MAX];
      int n = toolset_resolve(&registry, argv[1], resolved, TOOLSET_MAX_TOOLS, err, sizeof(err));
      if (n < 0)
      {
         fprintf(stderr, "toolset: %s\n", err[0] ? err : "failed to resolve toolset");
         return;
      }
      for (int i = 0; i < n; i++)
         fprintf(stdout, "%s\n", resolved[i]);
      return;
   }

   fprintf(stderr, "toolset: unknown subcommand '%s'\n\n", argv[0]);
   toolset_usage();
}
