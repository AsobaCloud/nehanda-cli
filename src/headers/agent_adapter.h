#ifndef DEC_AGENT_ADAPTER_H
#define DEC_AGENT_ADAPTER_H 1

#include "aimee.h"
#include "agent_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define AGENT_ADAPTER_CAP_DELEGATE_TURN        (1 << 0)
#define AGENT_ADAPTER_CAP_PRIMARY_CONVERSATION (1 << 1)
#define AGENT_ADAPTER_CAP_DIRECT_HTTP          (1 << 2)
#define AGENT_ADAPTER_CAP_PRIMARY_SESSION      (1 << 3)

   typedef struct agent_adapter
   {
      const char *name;
      const char *provider;
      const char *display_name;
      const char *default_endpoint;
      const char *default_model;
      const char *auth_type;
      int capabilities;
   } agent_adapter_t;

   const agent_adapter_t *agent_adapter_for_name(const char *name);
   const agent_adapter_t *agent_adapter_for_provider(const char *provider);
   const agent_adapter_t *agent_adapter_for_agent(const agent_t *agent);
   int agent_adapter_supports(const agent_adapter_t *adapter, int capability);
   int agent_adapter_agent_is_direct(const agent_t *agent);

#ifdef __cplusplus
}
#endif

#endif /* DEC_AGENT_ADAPTER_H */
