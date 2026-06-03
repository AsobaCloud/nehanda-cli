/* kb_background.h: aimee-kb autonomous-task slot tracker.
 *
 * Mirrors the aimee-server compute-pool slot tracking on the kb side:
 * background subsystems (curator drain, maintenance timer, etc.) call
 * kb_background_set(name, fmt, ...) when they begin a unit of work and
 * kb_background_clear(name) when they finish. The workers endpoint reads
 * the registry and surfaces it so `aimee workers` can render every
 * autonomous task aimee-kb is doing.
 *
 * The registry is bounded (KB_BACKGROUND_SLOT_MAX); attempts to set a
 * name not already present and not in a free slot are silently dropped.
 * Re-setting the same name in place is supported (subsystems update
 * their descriptor as work progresses).
 */
#ifndef DEC_KB_BACKGROUND_H
#define DEC_KB_BACKGROUND_H 1

#define KB_BACKGROUND_SLOT_MAX       8
#define KB_BACKGROUND_NAME_MAX       32
#define KB_BACKGROUND_DESCRIPTOR_MAX 128

/* Set this thread's-or-subsystem's slot to active with the given
 * descriptor. Safe to call repeatedly to update the descriptor; sets
 * started_at on the first transition from idle to active. */
void kb_background_set(const char *name, const char *descriptor_fmt, ...);

/* Clear a previously-set slot back to idle. Safe to call with a name
 * that is not active (no-op). */
void kb_background_clear(const char *name);

/* Return a heap-allocated JSON array. Caller frees. Shape:
 *   [{"name":"curator","active":true,"descriptor":"...","elapsed_secs":12}, ...]
 * Idle slots are omitted from the output to keep `aimee workers` tight.
 */
char *kb_background_slots_json(void);

/* Forward declaration: kb_service.c sends this; we build the body. */
struct cJSON;

/* Build the kb.workers response object (status, configured, slots,
 * background). conn_slots_json may be NULL; ownership is consumed.
 * Returns a heap-allocated cJSON object; caller takes ownership and is
 * responsible for cJSON_Delete. */
struct cJSON *kb_workers_response_build(int configured, char *conn_slots_json);

#endif /* DEC_KB_BACKGROUND_H */
