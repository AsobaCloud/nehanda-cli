/* posix/agent_max_turns.c: max-turn resolution for primary vs delegate sessions. */
#include "aimee.h"
#include "agent_exec.h"
#include "agent_types.h"
#include "config.h"

int agent_resolve_max_turns(const agent_t *agent, const char *role)
{
   /* max_turns > 0 on the agent only caps delegate runs; primary sessions
    * (role == NULL) are unlimited regardless of the configured value. */
   if (agent->max_turns > 0 && role)
      return agent->max_turns;
   if (agent->max_turns == 0)
      return 1000;
   config_t iter_cfg;
   config_load(&iter_cfg);
   if (role)
      return iter_cfg.max_iterations_delegate > 0 ? iter_cfg.max_iterations_delegate
                                                  : CONFIG_DEFAULT_MAX_ITERATIONS_DELEGATE;
   return iter_cfg.max_iterations > 0 ? iter_cfg.max_iterations : 1000;
}
