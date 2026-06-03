/* kb_curator_judge.h: deep-curator LLM judge sidecar.
 *
 * The resolve_entities NN search produces three outcomes by cosine score:
 *   >= 0.85          a confident merge (no judge needed)
 *   <  0.70          a confident create (no judge needed)
 *   [0.70, 0.85)     ambiguous — too close to create blindly, too far to merge
 *
 * This module adjudicates the ambiguous band with an LLM sidecar: it asks
 * whether the new mention and the nearest existing entity are the same
 * canonical thing. The sidecar reads a JSON request on stdin and writes a JSON
 * response on stdout, mirroring the curator-extract sidecar contract.
 * No DB access from this file. */
#ifndef KB_CURATOR_JUDGE_H
#define KB_CURATOR_JUDGE_H

#include <stddef.h>

/* Ask the judge sidecar whether `mention` refers to the same canonical entity
 * as `candidate`. Request JSON:
 *   {"task":"same_entity","mention":{"name":...,"context":...},
 *    "candidate":{"name":...},"score":<float>}
 * Expected response JSON: {"same_entity": true|false}.
 *
 * Returns 0 and sets *out_same to 0/1 on a clean decision. Returns -1 on any
 * error (empty command, spawn failure, non-zero exit, unparseable output) and
 * leaves *out_same untouched — the caller MUST treat a judge error as "not the
 * same entity" (i.e. create), never as a merge, so a flaky sidecar can only
 * over-create, never silently collapse distinct entities. */
int kb_curator_judge_same_entity(const char *judge_cmd, const char *mention_name,
                                 const char *mention_context, const char *candidate_name,
                                 double score, int *out_same, char *errbuf, size_t errlen);

#endif /* KB_CURATOR_JUDGE_H */
