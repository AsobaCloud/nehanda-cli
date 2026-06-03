/* openai_responses_store.h: in-process store for OpenAI responses conversation transcripts. */
#ifndef DEC_OPENAI_RESPONSES_STORE_H
#define DEC_OPENAI_RESPONSES_STORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Store (or overwrite) the accumulated transcript for a response id. The
    * transcript is copied (heap). No-op if resp_id or transcript is NULL/empty.
    * The store is bounded; when full the oldest slot is reused. */
   void openai_responses_store_put(const char *resp_id, const char *transcript);

   /* Look up the transcript for a previously stored response id. Copies it into
    * out[out_n] (NUL-terminated, truncated if needed). Returns 1 if found, 0
    * otherwise (and sets out[0]='\0' when out/out_n are valid). */
   int openai_responses_store_get(const char *resp_id, char *out, size_t out_n);

   /* Drop all entries (test helper). */
   void openai_responses_store_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* DEC_OPENAI_RESPONSES_STORE_H */
