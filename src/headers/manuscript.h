/* manuscript.h: helpers for `aimee manuscript` (novel-mode creative writing).
 *
 * Pure, side-effect-light helpers so they can be unit-tested directly; the
 * command dispatch lives in cmd_manuscript.c. */
#ifndef DEC_MANUSCRIPT_H
#define DEC_MANUSCRIPT_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MANUSCRIPT_MAX_FILES 1024
#define MANUSCRIPT_NAME_MAX  256

   typedef struct
   {
      char name[MANUSCRIPT_NAME_MAX]; /* file name relative to the scanned dir */
      long words;                     /* word count */
   } manuscript_entry_t;

   /* Count words: maximal runs of non-whitespace separated by whitespace.
    * NULL -> 0. */
   long manuscript_count_words(const char *text);

   /* True when name looks like a prose manuscript file (.md or .txt), is not
    * hidden, and is not the .aimee-rules story bible. */
   int manuscript_is_prose_file(const char *name);

   /* Scan dir (one level) for prose files, counting words in each. Results are
    * written to out (up to max), sorted by file name (so 01-, 02-, ... order),
    * and *total_out (when non-NULL) gets the summed word count. Returns the
    * number of files found, or 0 if dir cannot be read. */
   int manuscript_scan(const char *dir, manuscript_entry_t *out, int max, long *total_out);

   /* True when a continuity report signals an unresolved contradiction — used
    * by `aimee manuscript check` as a done-gate. Looks for the machine-readable
    * "CONTINUITY: FAIL" verdict line the continuity role emits. */
   int manuscript_continuity_failed(const char *report);

   /* Read a whole file into a heap buffer (NUL-terminated; caller frees).
    * Returns NULL on failure or for files larger than the per-scene cap. */
   char *manuscript_read_file(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* DEC_MANUSCRIPT_H */
