/* compact_prune.h: pre-summarization tool-result content pruning.
 *
 * Part of the pluggable context engine surface described in
 * docs/proposals/pending/plugin-extension-surface-and-context-engine.md (§7).
 *
 * Before building the compaction summary, large tool-result blobs are
 * replaced with a one-line stub:
 *   [read_file]   4,200 chars truncated
 *   [bash]        ran command -> 47 lines output (1,890 chars)
 *
 * This reduces the token cost of the LLM compaction summary pass without
 * losing structural information (the tool name and approximate size are kept).
 */
#ifndef DEC_COMPACT_PRUNE_H
#define DEC_COMPACT_PRUNE_H 1

struct cJSON;

#ifdef __cplusplus
extern "C"
{
#endif

/* Tool results whose serialised content exceeds this many bytes are pruned. */
#define COMPACT_PRUNE_THRESHOLD_BYTES 400

   /* Prune large tool-result content in messages[start_idx .. end_idx) in-place.
    * Each oversized result has its content replaced by a one-line stub.
    * The tool name is resolved from the paired tool-call message where possible.
    * threshold: minimum content bytes that triggers pruning (0 → use default).
    * Returns total content bytes saved (0 if nothing was pruned). */
   int compact_prune_tool_results(struct cJSON *messages, int start_idx, int end_idx,
                                  int threshold);

   /* Estimate what fraction of tokens would be saved by pruning.
    * Scans messages[start_idx .. end_idx), comparing serialised size with and
    * without pruning (does not modify messages).
    * Returns a value in [0.0, 1.0]; 0.0 means no prunable content. */
   double compact_prune_estimate_savings(struct cJSON *messages, int start_idx, int end_idx,
                                         int threshold);

#ifdef __cplusplus
}
#endif

#endif /* DEC_COMPACT_PRUNE_H */
