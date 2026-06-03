#include "aimee.h"
#include "agent.h"

size_t agent_exec_context_budget_chars(const agent_t *agent)
{
   if (!agent || agent->middleware.context_window <= 0)
      return AGENT_CONTEXT_BUDGET;

   int output_tokens = agent->max_tokens > 0 ? agent->max_tokens : AGENT_DEFAULT_MAX_TOKENS;
   int prompt_tokens = agent->middleware.context_window - output_tokens;
   if (prompt_tokens <= 0)
      prompt_tokens = agent->middleware.context_window / 2;
   if (prompt_tokens < 1024)
      prompt_tokens = 1024;

   size_t budget = (size_t)prompt_tokens * 4u;
   if (budget < 4096)
      budget = 4096;
   return budget;
}
