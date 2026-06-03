/* file_safety.c: read-before-write tracking and stale-edit detection */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "aimee.h"
#include <inttypes.h>

/* FNV-1a 64-bit hash of a file's full contents.
 * Reads in 4 KB chunks; returns 0 on open failure (or empty file). */
uint64_t file_content_hash(const char *path)
{
   if (!path || !path[0])
      return 0;
   FILE *f = fopen(path, "rb");
   if (!f)
      return 0;

   uint64_t h = 14695981039346656037ULL;
   unsigned char buf[4096];
   size_t n;
   while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
   {
      for (size_t i = 0; i < n; i++)
      {
         h ^= buf[i];
         h *= 1099511628211ULL;
      }
   }
   fclose(f);
   return h;
}

/* Return 1 if the file at path contains needle as a substring.
 * Loads the file up to a sane cap; returns 0 on open failure, empty
 * needle, or size overflow. Treats content as a null-terminated string,
 * so will miss matches past an embedded NUL — acceptable for source text. */
int file_contains_substring(const char *path, const char *needle)
{
   if (!path || !*path || !needle || !*needle)
      return 0;
   FILE *f = fopen(path, "rb");
   if (!f)
      return 0;
   if (fseek(f, 0, SEEK_END) != 0)
   {
      fclose(f);
      return 0;
   }
   long n = ftell(f);
   if (n < 0 || n > 64L * 1024L * 1024L)
   {
      fclose(f);
      return 0;
   }
   rewind(f);
   char *buf = malloc((size_t)n + 1);
   if (!buf)
   {
      fclose(f);
      return 0;
   }
   size_t got = fread(buf, 1, (size_t)n, f);
   buf[got] = '\0';
   fclose(f);
   int found = strstr(buf, needle) != NULL;
   free(buf);
   return found;
}

/* Return 1 if abs_path appears in the session read-set. */
int session_path_was_read(const session_state_t *state, const char *abs_path)
{
   if (!state || !abs_path)
      return 0;
   for (int i = 0; i < state->read_path_count; i++)
      if (strcmp(state->read_paths[i], abs_path) == 0)
         return 1;
   return 0;
}

/* Return the stored content hash for abs_path, or 0 if not recorded. */
uint64_t session_file_hash(const session_state_t *state, const char *abs_path)
{
   if (!state || !abs_path)
      return 0;
   for (int i = 0; i < state->file_hash_count; i++)
      if (strcmp(state->file_hashes[i].path, abs_path) == 0)
         return state->file_hashes[i].content_hash;
   return 0;
}

/* Record that abs_path was read: add to the read-set and refresh its hash.
 * Both the path list and hash table are capped; entries beyond the cap are
 * silently dropped rather than blocking the session. */
void session_record_read(session_state_t *state, const char *abs_path)
{
   if (!state || !abs_path || !abs_path[0])
      return;

   if (!session_path_was_read(state, abs_path) && state->read_path_count < MAX_READ_PATHS)
   {
      snprintf(state->read_paths[state->read_path_count], MAX_SEEN_LEN, "%s", abs_path);
      state->read_path_count++;
      state->dirty = 1;
   }

   uint64_t h = file_content_hash(abs_path);
   for (int i = 0; i < state->file_hash_count; i++)
   {
      if (strcmp(state->file_hashes[i].path, abs_path) == 0)
      {
         state->file_hashes[i].content_hash = h;
         state->dirty = 1;
         return;
      }
   }
   if (state->file_hash_count < MAX_FILE_HASHES)
   {
      snprintf(state->file_hashes[state->file_hash_count].path, MAX_SEEN_LEN, "%s", abs_path);
      state->file_hashes[state->file_hash_count].content_hash = h;
      state->file_hash_count++;
      state->dirty = 1;
   }
}
