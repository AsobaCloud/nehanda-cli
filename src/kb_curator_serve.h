/* kb_curator_serve.h: read-side serving for the deep-curator artifact graph.
 *
 * Thin DB-graph traversals behind the dedicated curator HTTP endpoints. The
 * curator passes write the graph (code_unit --mentions--> entity, synthesis
 * --about/--cites--> ..., claim --contradicts--> claim); these turn a topic into
 * an answer. No DB1 access. */
#ifndef DEC_KB_CURATOR_SERVE_H
#define DEC_KB_CURATOR_SERVE_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* POST /v1/implements {"topic": "..."} — code units implementing a topic:
    * code_units that `mention` an entity whose name is the topic, or that
    * `mention` an entity cited by a document whose path contains the topic.
    * Emits {"topic":...,"implements":[{code_unit_id,file_path,summary}...]}.
    * Returns the count (>= 0), or -1 on bad input / no DB. */
   int kb_curator_implements_json(const char *topic, char *out, size_t out_cap);

   /* POST /v1/synthesize {"topic": "..."} — the latest synthesis narrative whose
    * topic entity name matches, with its cited sources. Emits
    * {"topic":...,"synthesis_id":...,"text":...,"sources":[...]}. Returns the
    * cited-source count (>= 0), or -1 on bad input / no DB / none. */
   int kb_curator_synthesize_serve_json(const char *topic, char *out, size_t out_cap);

   /* POST /v1/contradictions — committed `contradicts` claim pairs (most recent
    * first). Emits {"contradictions":[{a,b,a_text,b_text}...]}. limit is clamped
    * to [1,100]. Returns the pair count (>= 0), or -1 on no DB. */
   int kb_curator_contradictions_json(int limit, char *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* DEC_KB_CURATOR_SERVE_H */
