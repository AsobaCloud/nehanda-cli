/* tool_args_coerce.h: best-effort coercion of model-emitted tool-call
 * arguments against their declared JSON schema. Implements section 4 of
 * docs/proposals/pending/provider-profile-registry-and-auxiliary-models.md. */
#ifndef DEC_TOOL_ARGS_COERCE_H
#define DEC_TOOL_ARGS_COERCE_H 1

#include "cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

   /* Returns a NEWLY-ALLOCATED cJSON value the caller must cJSON_Delete.
    * Never modifies inputs. NULL only on allocation failure; otherwise
    * returns at minimum a deep clone of raw_args. */
   cJSON *tool_args_coerce(const cJSON *declared_schema, const cJSON *raw_args);

#ifdef __cplusplus
}
#endif

#endif /* DEC_TOOL_ARGS_COERCE_H */
