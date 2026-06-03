/* kb_intel_payload.h: shared JSON builders for intelligence readiness routes. */
#ifndef DEC_KB_INTEL_PAYLOAD_H
#define DEC_KB_INTEL_PAYLOAD_H 1

#include "cJSON.h"

cJSON *kb_intel_calibrate_readiness_response(void);
cJSON *kb_intel_demote_check_response(void);
cJSON *kb_intel_bandit_export_response(void);

/* Record the offline-replay result (output of tools/bandit_replay.py) as a
 * benchmark_trace artifact. body_json is {decision_point, result} — `result`
 * is the replay-tool output object.
 * Returns a response object (caller frees). */
cJSON *kb_intel_bandit_replay_record_response(const char *body_json, int body_len);

/* HTTP wrapper around the response builder: writes JSON into out_buf and
 * returns the HTTP status. */
int kb_intel_bandit_replay_record_http(const char *body, int body_len, char *out_buf, int out_cap);

#endif /* DEC_KB_INTEL_PAYLOAD_H */
