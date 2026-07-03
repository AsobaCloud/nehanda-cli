#include "kb_doc_hash.h"

#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void hex_digest(const unsigned char *digest, char out[KB_DOC_HASH_HEX_LEN + 1])
{
   for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
      snprintf(out + i * 2, 3, "%02x", digest[i]);
   out[KB_DOC_HASH_HEX_LEN] = '\0';
}

void kb_doc_content_hash(const char *bytes, int nbytes, char out[KB_DOC_HASH_HEX_LEN + 1])
{
   unsigned char digest[SHA256_DIGEST_LENGTH];

   if (!out)
      return;

   if (!bytes || nbytes < 0)
      nbytes = 0;

   SHA256((const unsigned char *)(bytes ? bytes : ""), (size_t)nbytes, digest);
   hex_digest(digest, out);
}

/* The frontmatter keys whose changes must NOT invalidate the extraction cache. */
static int fm_key_is_ignorable(const char *line, size_t len)
{
   static const char *keys[] = {"status", "reviewed", "tags", "date"};
   /* key = leading run of non-space, non-':' chars at column 0 (top-level key). */
   size_t k = 0;
   while (k < len && line[k] != ':' && line[k] != ' ' && line[k] != '\t' && line[k] != '\n' &&
          line[k] != '\r')
      k++;
   if (k == 0 || k >= len || line[k] != ':')
      return 0; /* not a "key:" line */
   for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
      if (strlen(keys[i]) == k && strncmp(line, keys[i], k) == 0)
         return 1;
   return 0;
}

void kb_doc_content_hash_md(const char *bytes, int nbytes, char out[KB_DOC_HASH_HEX_LEN + 1])
{
   if (!out)
      return;
   if (!bytes || nbytes < 0)
      nbytes = 0;

   /* No leading `---` frontmatter → identical to the plain content hash. The block
    * delimiter is exactly "---" on the first line (optionally with a CR). */
   size_t n = (size_t)nbytes;
   int has_fm = (n >= 3 && bytes[0] == '-' && bytes[1] == '-' && bytes[2] == '-' &&
                 (n == 3 || bytes[3] == '\n' || bytes[3] == '\r'));
   if (!has_fm)
   {
      kb_doc_content_hash(bytes, nbytes, out);
      return;
   }

   char *buf = malloc(n + 1);
   if (!buf)
   {
      kb_doc_content_hash(bytes, nbytes, out); /* fail safe: exact hash */
      return;
   }
   size_t w = 0;
   size_t i = 0;
   int in_fm = 0;    /* 0=before first ---, 1=inside frontmatter, 2=body */
   int skipping = 0; /* dropping an ignorable key's line + its indented continuation */
   while (i < n)
   {
      size_t start = i;
      while (i < n && bytes[i] != '\n')
         i++;
      if (i < n)
         i++; /* include the newline in [start,i) */
      const char *line = bytes + start;
      size_t llen = i - start;
      /* trim a trailing CR for the "---" comparison */
      size_t clen = llen;
      while (clen && (line[clen - 1] == '\n' || line[clen - 1] == '\r'))
         clen--;

      int is_delim = (clen == 3 && line[0] == '-' && line[1] == '-' && line[2] == '-');
      if (in_fm == 0)
      {
         in_fm = 1; /* the opening --- */
         memcpy(buf + w, line, llen);
         w += llen;
         continue;
      }
      if (in_fm == 1)
      {
         if (is_delim)
         {
            in_fm = 2; /* closing --- */
            memcpy(buf + w, line, llen);
            w += llen;
            continue;
         }
         int indented = (llen > 0 && (line[0] == ' ' || line[0] == '\t'));
         if (indented)
         {
            if (skipping)
               continue; /* drop a continuation of an ignorable key */
         }
         else
         {
            skipping = fm_key_is_ignorable(line, clen);
            if (skipping)
               continue; /* drop the ignorable key line */
         }
         memcpy(buf + w, line, llen);
         w += llen;
         continue;
      }
      /* body */
      memcpy(buf + w, line, llen);
      w += llen;
   }

   unsigned char digest[SHA256_DIGEST_LENGTH];
   SHA256((const unsigned char *)buf, w, digest);
   free(buf);
   hex_digest(digest, out);
}

/* Case-insensitive suffix test. */
static int ends_with_ci(const char *s, const char *suffix)
{
   size_t ls = strlen(s), lx = strlen(suffix);
   if (lx > ls)
      return 0;
   const char *p = s + (ls - lx);
   for (size_t i = 0; i < lx; i++)
   {
      char a = p[i], b = suffix[i];
      if (a >= 'A' && a <= 'Z')
         a = (char)(a - 'A' + 'a');
      if (b >= 'A' && b <= 'Z')
         b = (char)(b - 'A' + 'a');
      if (a != b)
         return 0;
   }
   return 1;
}

void kb_doc_content_hash_for_path(const char *path, const char *bytes, int nbytes,
                                  char out[KB_DOC_HASH_HEX_LEN + 1])
{
   if (path &&
       (ends_with_ci(path, ".md") || ends_with_ci(path, ".mdx") || ends_with_ci(path, ".markdown")))
      kb_doc_content_hash_md(bytes, nbytes, out);
   else
      kb_doc_content_hash(bytes, nbytes, out);
}
