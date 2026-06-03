#ifndef DEC_DELEGATE_CREDENTIAL_RETRY_H
#define DEC_DELEGATE_CREDENTIAL_RETRY_H 1

#include <stddef.h>

#include "aimee.h"
#include "agent_types.h"

int delegate_run_with_credential_retry(agent_config_t *cfg, agent_t *agent, const char *role,
                                       const char *system_prompt, const char *run_prompt,
                                       int max_tokens, int force_tools, int enforce_writes,
                                       char *leased_cred_name, size_t leased_cred_name_cap,
                                       const char *credential_state_path, agent_result_t *result);

#endif /* DEC_DELEGATE_CREDENTIAL_RETRY_H */
