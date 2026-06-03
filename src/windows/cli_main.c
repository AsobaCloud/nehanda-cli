/* cli_main.c: Windows-specific provider CLI helpers */
#include "platform.h"
#include <string.h>

const char *platform_cli_autonomous_flag(const char *provider)
{
   if (!provider || !provider[0])
      return NULL;
   if (strcmp(provider, "claude") == 0 || strcmp(provider, "claude-code") == 0)
      return "--dangerously-skip-permissions";
   if (strcmp(provider, "codex") == 0)
      return "--dangerously-bypass-approvals-and-sandbox";
   return NULL;
}
