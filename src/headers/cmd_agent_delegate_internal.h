#ifndef CMD_AGENT_DELEGATE_INTERNAL_H
#define CMD_AGENT_DELEGATE_INTERNAL_H
#include "cmd_agent_delegate_impl.h"
/* Cross-TU decls split from cmd_agent_delegate.c (was cmd_agent_delegate_toolset.inc). */
/* promoted cross-TU (former .inc statics) */
const char *delegate_toolset_override_arg(const opt_parsed_t *opts, int argc, char **argv,
                                          const char **prompt_io);

#endif
