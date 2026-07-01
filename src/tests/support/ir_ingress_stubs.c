/* ir_ingress_stubs.c -- WEAK no-op stubs for the IR/router hooks wired into
 * anthropic_http.c / openai_chat.c. The minimal-link ingress tests (#include the
 * ingress .c to exercise tool-policing / memory / SSE) don't test the IR path, so
 * they don't need the real (heavy) router_advise + IR chain linked. These stubs are
 * WEAK: if a test does link the real objects, the strong definitions win. */
#include <stddef.h>

__attribute__((weak)) int gw_stage_router(void *r, void *ud)
{
   (void)r;
   (void)ud;
   return 0;
}

__attribute__((weak)) void aimee_ir_shadow_observe_request(const void *req, int frontend)
{
   (void)req;
   (void)frontend;
}

__attribute__((weak)) int aimee_ir_path_enabled(void)
{
   return 0;
}

__attribute__((weak)) char *aimee_ir_build_provider_body(const void *req, const char *driver_name,
                                                         const char *agent_model,
                                                         int max_tokens_override)
{
   (void)req;
   (void)driver_name;
   (void)agent_model;
   (void)max_tokens_override;
   return NULL;
}

__attribute__((weak)) int aimee_ir_responses_to_chat(const char *body, char *model, size_t model_n,
                                                     char **instructions_out, void **messages_out,
                                                     void **tools_out, int *stream_out)
{
   (void)body;
   (void)model;
   (void)model_n;
   (void)instructions_out;
   (void)messages_out;
   (void)tools_out;
   (void)stream_out;
   return -1;
}

__attribute__((weak)) void *aimee_ir_build_from_chat(const char *agent_model, const void *messages,
                                                     const void *tools, const char *system,
                                                     const char *driver_name)
{
   (void)agent_model;
   (void)messages;
   (void)tools;
   (void)system;
   (void)driver_name;
   return NULL;
}
