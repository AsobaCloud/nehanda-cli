/* db1/wfe_binding.h: interactive session <-> work-item binding (S2 primary-as-
 * manager). Single-writer: one binding per session; a second session binding the
 * same work-item is refused. Backend access stays private to src/db1/. */
#ifndef DEC_DB1_WFE_BINDING_H
#define DEC_DB1_WFE_BINDING_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Bind session -> work_item. Idempotent: re-binding the SAME session succeeds
    * (enforce_stage is NOT changed on re-bind -> monotonic per session row).
    * Single-writer: refuse (return -2) if work_item is already bound to a
    * DIFFERENT session. Returns 0 on success, -2 on single-writer conflict, -1 on
    * error / bad args. */
   int db1_wfe_bind(const char *session_id, const char *work_item_id, const char *enforce_stage);

   /* Look up a session's binding. Returns 1 if bound (fills wi_out + stage_out), 0
    * if not bound, -1 on error. Either out buffer may be NULL. */
   int db1_wfe_binding_get(const char *session_id, char *wi_out, size_t wi_n, char *stage_out,
                           size_t stage_n);

   /* Release a session's binding (orphan / rebind). Returns 0 (incl. no-op), -1 on
    * error. */
   int db1_wfe_unbind(const char *session_id);

#ifdef __cplusplus
}
#endif

#endif /* DEC_DB1_WFE_BINDING_H */
