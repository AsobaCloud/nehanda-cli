/* delegate_credential_retry.c: same-request retry within a credential pool. */

#include "delegate_credential_retry.h"

#include "agent_exec.h"
#include "delegate_credentials.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int run_delegate_attempt(agent_config_t *cfg, const char *role, const char *system_prompt,
                                const char *run_prompt, int max_tokens, int force_tools,
                                int enforce_writes, agent_result_t *result)
{
   if (force_tools)
      return agent_run_with_tools_write_enforce(cfg, role, system_prompt, run_prompt, max_tokens,
                                                enforce_writes, result);
   return agent_run(cfg, role, system_prompt, run_prompt, max_tokens, result);
}

static int result_failed_for_pool(const agent_result_t *result, int rc)
{
   return rc != 0 || !result || !result->success;
}

static void result_add_prior_metrics(agent_result_t *result, const agent_result_t *prior)
{
   if (!result || !prior)
      return;
   result->turns += prior->turns;
   result->tool_calls += prior->tool_calls;
   result->prompt_tokens += prior->prompt_tokens;
   result->completion_tokens += prior->completion_tokens;
   result->latency_ms += prior->latency_ms;
}

int delegate_run_with_credential_retry(agent_config_t *cfg, agent_t *agent, const char *role,
                                       const char *system_prompt, const char *run_prompt,
                                       int max_tokens, int force_tools, int enforce_writes,
                                       char *leased_cred_name, size_t leased_cred_name_cap,
                                       const char *credential_state_path, agent_result_t *result)
{
   if (!cfg || !result)
      return -1;

   int rc = run_delegate_attempt(cfg, role, system_prompt, run_prompt, max_tokens, force_tools,
                                 enforce_writes, result);
   if (!agent || agent->credential_count <= 1 || !leased_cred_name || !leased_cred_name[0])
      return rc;

   for (int attempt = 1; attempt < agent->credential_count && result_failed_for_pool(result, rc);
        attempt++)
   {
      agent_result_t prior = *result;
      char next_env[MAX_CRED_ENV_VAR_LEN] = "";
      int rotated = delegate_credentials_rotate_after_failure(
          agent->name, agent->credentials, agent->credential_count, agent->provider,
          leased_cred_name, leased_cred_name_cap, next_env, sizeof(next_env), result->error,
          time(NULL));
      if (rotated != 1)
         break;
      if (credential_state_path && credential_state_path[0])
         (void)delegate_credentials_save_file(credential_state_path);

      const char *value = getenv(next_env);
      agent->api_key[0] = '\0';
      if (value && value[0])
         snprintf(agent->api_key, sizeof(agent->api_key), "%s", value);

      memset(result, 0, sizeof(*result));
      rc = run_delegate_attempt(cfg, role, system_prompt, run_prompt, max_tokens, force_tools,
                                enforce_writes, result);
      result_add_prior_metrics(result, &prior);
      free(prior.response);
   }

   return rc;
}
