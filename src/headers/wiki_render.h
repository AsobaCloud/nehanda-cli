/* wiki_render.h: deterministic markdown projection of the memory store. */
#ifndef DEC_WIKI_RENDER_H
#define DEC_WIKI_RENDER_H 1

#include "memory.h"

#ifdef __cplusplus
extern "C"
{
#endif

   /* Write a deterministic wiki into |out_dir|.  Creates the directory if it
    * does not exist.  Returns 0 on success, -1 on I/O error.
    * |mems| is grouped by kind before writing; the caller may pass NULL to
    * trigger a live fetch via kb_client. */
   int wiki_render(const char *out_dir);

#ifdef __cplusplus
}
#endif

#endif /* DEC_WIKI_RENDER_H */
