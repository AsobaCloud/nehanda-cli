/* prompt_sanitizer.c: see prompt_sanitizer.h. Pure, no DB/network. */
#include "prompt_sanitizer.h"

#include <stddef.h>
#include <string.h>

/* Per-kind output bound (excluding NUL). Structured kinds are tight; free-text
 * kinds are generous but still bounded so a giant field can't blow the prompt. */
static const size_t kKindBound[SANITIZE_KIND_COUNT] = {
    [SANITIZE_SYMBOL_LABEL] = 512,     [SANITIZE_FILE_PATH] = 1024,
    [SANITIZE_SOURCE_LOCATION] = 1088, [SANITIZE_COMMUNITY_NAME] = 256,
    [SANITIZE_MEMORY_FACT] = 4096,     [SANITIZE_LESSON_TEXT] = 2048,
    [SANITIZE_CORRECTION] = 4096,      [SANITIZE_IMAGE_CAPTION] = 2048,
    [SANITIZE_TRANSCRIPT] = 16384,     [SANITIZE_MARKDOWN_DOC] = 65536,
};

size_t sanitize_kind_bound(sanitize_kind_t kind)
{
   if (kind < 0 || kind >= SANITIZE_KIND_COUNT)
      return 0;
   return kKindBound[kind];
}

int sanitize_kind_is_structured(sanitize_kind_t kind)
{
   return kind >= 0 && kind < SANITIZE_MEMORY_FACT;
}

/* Enumerated injection markers (opening delimiters). Matched case-insensitively.
 * In a STRUCTURED field any occurrence is a hard reject; in FREE TEXT each is
 * defanged in place by inserting a space after its first byte, breaking the
 * special-token / role-tag / directive tokenization while staying readable.
 * Kept as a table so the S0 attack corpus and the PR's marker enumeration stay
 * in sync with the code. */
static const char *const kMarkers[] = {
    "<|",
    "|>", /* special-token delimiters: <|im_start|>, <|endoftext|>, ...   */
    "<system",
    "</system",
    "<assistant",
    "</assistant",
    "<user",
    "</user",
    "<tool",
    "</tool",
    "<human",
    "</human", /* role tags */
    "### instruction",
    "### system",
    "### response",
    "### human",
    "### assistant", /* instruction headers */
    "[graphify]",
    "[system]",
    "[assistant]",
    "[inst]",
    "[/inst]", /* bracket tags */
};
static const size_t kMarkerCount = sizeof(kMarkers) / sizeof(kMarkers[0]);

static char lc(unsigned char c)
{
   return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
}

/* Length of the marker matching at s (case-insensitive), or 0 if none. */
static size_t marker_at(const char *s)
{
   for (size_t m = 0; m < kMarkerCount; m++)
   {
      const char *mk = kMarkers[m];
      size_t i = 0;
      for (; mk[i]; i++)
         if (lc((unsigned char)s[i]) != lc((unsigned char)mk[i]) || s[i] == '\0')
            break;
      if (mk[i] == '\0')
         return i; /* full marker matched */
   }
   return 0;
}

/* A C0/C1/DEL control byte we always drop (free text) or reject on (structured).
 * Newline (0x0A) / tab (0x09) are handled separately — legal in free text. */
static int is_hard_control(unsigned char c)
{
   return (c < 0x20 && c != 0x0A && c != 0x09) || c == 0x7F;
}

/* Back off `n` so out[0..n) does not end mid-UTF-8-sequence, then NUL-terminate. */
static void utf8_trim(char *out, size_t *n)
{
   size_t k = *n;
   while (k > 0 && ((unsigned char)out[k - 1] & 0xC0) == 0x80)
      k--; /* drop trailing continuation bytes */
   if (k > 0)
   {
      unsigned char lead = (unsigned char)out[k - 1];
      size_t need = 0;
      if ((lead & 0xE0) == 0xC0)
         need = 2;
      else if ((lead & 0xF0) == 0xE0)
         need = 3;
      else if ((lead & 0xF8) == 0xF0)
         need = 4;
      if (need && (*n - (k - 1)) < need)
         k--; /* incomplete multibyte lead — drop it */
   }
   out[k] = '\0';
   *n = k;
}

/* STRUCTURED: reject on any control/newline/ANSI/injection marker; otherwise copy
 * verbatim (already clean), TRUNCATED if it exceeds the effective bound. */
static sanitize_status_t sanitize_structured(const char *field, size_t maxlen, char *out,
                                             sanitize_reason_t *reason)
{
   for (const char *p = field; *p;)
   {
      unsigned char c = (unsigned char)*p;
      if (c == 0x0A || c == 0x0D)
         return (*reason = SANITIZE_REASON_NEWLINE), SANITIZE_REJECTED;
      if (c == 0x1B || is_hard_control(c) || c == 0x09)
         return (*reason = SANITIZE_REASON_CONTROL_CHAR), SANITIZE_REJECTED;
      if (c == 0xC2 && ((unsigned char)p[1] >= 0x80 && (unsigned char)p[1] <= 0x9F))
         return (*reason = SANITIZE_REASON_CONTROL_CHAR), SANITIZE_REJECTED; /* C1 */
      if (marker_at(p))
         return (*reason = SANITIZE_REASON_INJECTION_MARK), SANITIZE_REJECTED;
      p++;
   }
   size_t len = strlen(field);
   sanitize_status_t st = SANITIZE_OK;
   if (len > maxlen)
   {
      len = maxlen;
      st = SANITIZE_TRUNCATED;
      *reason = SANITIZE_REASON_LENGTH;
   }
   memcpy(out, field, len);
   size_t n = len;
   if (st == SANITIZE_TRUNCATED)
      utf8_trim(out, &n);
   else
      out[n] = '\0';
   return st;
}

/* FREE TEXT: strip control/ANSI/C1 (keep newline/tab), defang markers, bound
 * length. Never rejected. */
static sanitize_status_t sanitize_freetext(const char *field, size_t maxlen, char *out,
                                           sanitize_reason_t *reason)
{
   size_t n = 0;
   int truncated = 0;
   for (const char *p = field; *p;)
   {
      unsigned char c = (unsigned char)*p;
      /* ANSI escape sequence: ESC then CSI/OSC/simple — drop the whole run. */
      if (c == 0x1B)
      {
         p++;
         if (*p == '[') /* CSI: ESC [ ... final 0x40-0x7E */
         {
            p++;
            while (*p && !((unsigned char)*p >= 0x40 && (unsigned char)*p <= 0x7E))
               p++;
            if (*p)
               p++;
         }
         else if (*p == ']') /* OSC: ESC ] ... BEL or ST */
         {
            p++;
            while (*p && (unsigned char)*p != 0x07 && !((unsigned char)*p == 0x1B && p[1] == '\\'))
               p++;
            if ((unsigned char)*p == 0x1B && p[1] == '\\')
               p += 2;
            else if (*p)
               p++;
         }
         else if (*p)
            p++; /* simple ESC x */
         continue;
      }
      if (c == 0xC2 && ((unsigned char)p[1] >= 0x80 && (unsigned char)p[1] <= 0x9F))
      {
         p += 2; /* UTF-8-encoded C1 control — drop */
         continue;
      }
      if (is_hard_control(c))
      {
         p++; /* strip C0/DEL (newline/tab already excluded by is_hard_control) */
         continue;
      }
      /* Defang an injection marker: emit its first byte then a space, re-scan. */
      if (marker_at(p))
      {
         if (n + 2 > maxlen)
         {
            truncated = 1;
            break;
         }
         out[n++] = *p++;
         out[n++] = ' ';
         continue;
      }
      if (n + 1 > maxlen)
      {
         truncated = 1;
         break;
      }
      out[n++] = (char)c;
      p++;
   }
   if (truncated)
   {
      utf8_trim(out, &n);
      *reason = SANITIZE_REASON_LENGTH;
      return SANITIZE_TRUNCATED;
   }
   out[n] = '\0';
   return SANITIZE_OK;
}

sanitize_status_t sanitize_for_prompt(const char *field, sanitize_kind_t kind, char *out,
                                      size_t out_len, sanitize_reason_t *out_reason)
{
   sanitize_reason_t local = SANITIZE_REASON_NONE;
   sanitize_reason_t *reason = out_reason ? out_reason : &local;
   *reason = SANITIZE_REASON_NONE;

   if (!out || out_len == 0 || kind < 0 || kind >= SANITIZE_KIND_COUNT)
   {
      if (out && out_len)
         out[0] = '\0';
      *reason = SANITIZE_REASON_BAD_ARG;
      return SANITIZE_REJECTED;
   }
   out[0] = '\0';
   if (!field)
      return SANITIZE_OK; /* empty is clean */

   /* Effective bound = min(per-kind cap, caller buffer capacity). */
   size_t maxlen = kKindBound[kind];
   if (out_len - 1 < maxlen)
      maxlen = out_len - 1;

   if (sanitize_kind_is_structured(kind))
      return sanitize_structured(field, maxlen, out, reason);
   return sanitize_freetext(field, maxlen, out, reason);
}
