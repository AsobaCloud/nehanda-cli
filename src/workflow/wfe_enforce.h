/* wfe_enforce.h -- S2 pure enforcement policy cores: the per-block tool surface
 * (which tools a bound session may use while a workflow block is current) and
 * the rollout fail-class split (instrumentation-failure vs policy-decision). No
 * engine / DB / gateway deps, so the decisions are unit-testable in isolation;
 * the binding lookup, the ingress snapshot, and the actual tool stripping are
 * integration layers on top. Design per the S2 roundtable consult (2026-07-01). */
#ifndef DEC_WFE_ENFORCE_H
#define DEC_WFE_ENFORCE_H 1

#include "wfe_iface.h" /* wfe_block_type_t */

/* The tool surface a bound session gets while a block is current; each level is
 * strictly broader than the previous. */
typedef enum
{
   WFE_SURFACE_READONLY = 0, /* read/search only: no write/edit/patch, no delegate,
                                no deliver primitive. understand/split/review/gates,
                                converse/research. */
   WFE_SURFACE_DELEGATE,     /* + delegate-launch; still no direct write / deliver.
                                implement/document -- the primary delegates, it does
                                not write the tree itself. */
   WFE_SURFACE_FULL          /* unrestricted, EXCEPT the deliver gate still applies
                                pre-delivery. non-enforced / unknown blocks. */
} wfe_tool_surface_t;

/* Default surface for a block type. A bound enforced session uses this unless the
 * block's YAML declares an explicit allowed-tool list (S2 integration layer). */
wfe_tool_surface_t wfe_block_default_surface(wfe_block_type_t t);

/* 1 if `tool_name` may be used under `surface` given the run's `delivered` state.
 * A deliver primitive is denied until delivered at EVERY surface; a write tool is
 * denied below FULL; a delegate-launch tool is denied under READONLY. NULL tool
 * -> denied (fail closed). */
int wfe_surface_allows(wfe_tool_surface_t surface, const char *tool_name, int delivered);

/* 1 if `tool_name` mutates the working tree (write/edit/patch/apply/notebook). */
int wfe_is_write_tool(const char *tool_name);
/* 1 if `tool_name` launches a delegate / sub-agent. */
int wfe_is_delegate_tool(const char *tool_name);

/* ---- rollout fail-class split (consult Q5) ---- */
typedef enum
{
   WFE_FAIL_INSTRUMENTATION = 0, /* could not DETERMINE policy: binding lookup threw,
                                    DB unreachable, eval error. */
   WFE_FAIL_POLICY               /* policy DECIDED no (gate not passed / tool denied) --
                                    NOT an error. */
} wfe_fail_class_t;

typedef enum
{
   WFE_ACT_FAIL_OPEN_CHAT = 0, /* allow the turn as normal unbound chat (+ loud log) */
   WFE_ACT_FAIL_CLOSED         /* refuse the action */
} wfe_fail_action_t;

/* Decide what to do on a failure. `hard` = the enforcement dial is at hard.
 * `is_deliver_or_write` = the action being decided is a delivery/externalization/
 * write primitive (vs plain chat text). A POLICY denial ALWAYS fails closed. An
 * INSTRUMENTATION failure fails open to preserve chat, but fails CLOSED for a
 * deliver/write primitive when the dial is hard (so a lookup error cannot bypass
 * delivery enforcement). */
wfe_fail_action_t wfe_enforce_fail_action(wfe_fail_class_t cls, int hard, int is_deliver_or_write);

#endif /* DEC_WFE_ENFORCE_H */
