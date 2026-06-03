/* history_impl.h: private struct definition shared between history.c and
 * platform implementations (posix/history.c, windows/history.c). */
#ifndef DEC_HISTORY_IMPL_H
#define DEC_HISTORY_IMPL_H 1

#include "history.h"
#include <stdlib.h>

#ifndef HISTORY_PATH_MAX
#define HISTORY_PATH_MAX 4096
#endif

struct chat_history
{
   char **entries; /* ring buffer of heap-allocated strings  */
   int cap;        /* max entries                             */
   int count;      /* number of valid entries                 */
   int head;       /* index of oldest entry in the ring       */
   int nav_idx;    /* current nav position (count = at tip)  */
   char *saved;    /* unsaved current line preserved on nav  */
   char path[HISTORY_PATH_MAX];

   /* Tab completion: externally owned list of candidate words.
    * Caller must keep the array and strings alive while h is open. */
   const char **completions;    /* pointer array of candidate words     */
   int completion_count;        /* number of candidates                 */
   int completion_idx;          /* cycling position; -1 = not cycling   */
   char completion_prefix[256]; /* the prefix being completed        */

   /* Vim editing mode: when enabled, Esc enters normal mode. */
   int vim_mode_enabled;
};

#endif /* DEC_HISTORY_IMPL_H */
