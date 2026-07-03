#ifndef SERVER_PIPELINE_INTERNAL_H
#define SERVER_PIPELINE_INTERNAL_H
#include "server.h"
#include "roundtable_pipeline.h"
/* Cross-TU decls split from server_pipeline.c (was server_pipeline_merge.inc). */
/* promoted cross-TU (former .inc statics) */
void advance_after_merge(int id, rtp_run_t *run, rtp_gate_t *gate, int gate_no,
                                const char *merge_sha, cJSON *resp);
void execute_gate_merge(int id, rtp_run_t *run, rtp_gate_t *gate, int gate_no, cJSON *req,
                               cJSON *resp);
int validate_pr_for_merge(rtp_run_t *run, rtp_gate_t *gate, int gate_no, cJSON *resp);
int git_rev_parse(const rtp_run_t *run, const char *ref, char *out, size_t cap);
int prepare_impl_workspace(rtp_run_t *run, const char *merge_sha);
size_t rtp_cd_prefix(const rtp_run_t *run, char *buf, size_t cap);
const char *rtp_git_cwd(const rtp_run_t *run);

#endif
