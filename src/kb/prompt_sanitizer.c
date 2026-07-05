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
 * In a STRUCTURED field any occurrence is a hard reject; in FREE TEXT each whole
 * matched marker is replaced by a single space (after canonicalization), which
 * cannot reconstitute a marker across the replacement boundary. Kept as a table
 * so the S0 attack corpus and the PR's marker enumeration stay in sync. */
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

/* If out[0..*n) ends mid-UTF-8-sequence (a truncation split a multibyte char),
 * drop that trailing partial character; a COMPLETE final character is kept as-is.
 * Only ever called after a length truncation. NUL-terminates. */
static void utf8_trim(char *out, size_t *n)
{
   size_t k = *n;
   if (k == 0)
   {
      out[0] = '\0';
      return;
   }
   /* Walk back to the start of the last character (over continuation bytes). */
   size_t start = k;
   while (start > 0 && ((unsigned char)out[start - 1] & 0xC0) == 0x80)
      start--;
   if (start == 0)
   {
      /* No lead byte at all — the tail is pure continuation bytes (invalid). */
      out[0] = '\0';
      *n = 0;
      return;
   }
   start--; /* index of the last character's lead byte */
   unsigned char lead = (unsigned char)out[start];
   size_t need = 1; /* ASCII / invalid-lead: treat as a single byte */
   if ((lead & 0x80) == 0)
      need = 1;
   else if ((lead & 0xE0) == 0xC0)
      need = 2;
   else if ((lead & 0xF0) == 0xE0)
      need = 3;
   else if ((lead & 0xF8) == 0xF0)
      need = 4;
   size_t have = k - start;
   if (have >= need)
   {
      out[k] = '\0'; /* last character is complete — keep everything */
      return;
   }
   out[start] = '\0'; /* last character is incomplete — drop it whole */
   *n = start;
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

/* FREE TEXT: never rejected. Two passes, in the caller buffer, no alloc.
 *
 * Pass 1 CANONICALIZES: strip ANSI (CSI/OSC/simple), C0/DEL controls, and
 * UTF-8-encoded C1 controls — keeping newline/tab — into out[], bounded by maxlen.
 * Pass 2 DEFANGS the canonicalized text IN PLACE: every enumerated injection
 * marker is replaced by a single space.
 *
 * Canonicalize-BEFORE-defang is the fix for the control-split bypass (roundtable
 * R1): detecting markers on the raw input while stripping controls in the same
 * pass would let `<\x01|im_start|\x01>` reconstitute a live `<|im_start|>` after
 * the control bytes are removed. Here the controls are gone before any marker is
 * matched. Whole-marker→space replacement (rather than a first-byte split) cannot
 * reconstitute a marker across a replacement boundary, because a space is not the
 * first byte of any marker and any contiguous `<|`/role-tag in the canonicalized
 * text is always detected. The replacement shrinks (L bytes → 1), so the in-place
 * rewrite's write cursor never overtakes its read cursor. */
static sanitize_status_t sanitize_freetext(const char *field, size_t maxlen, char *out,
                                           sanitize_reason_t *reason)
{
   /* Pass 1: canonicalize into out. */
   size_t w = 0;
   int truncated = 0;
   for (const char *p = field; *p;)
   {
      unsigned char c = (unsigned char)*p;
      if (c == 0x1B) /* ANSI escape run — drop entirely */
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
         else if (*p == ']') /* OSC: ESC ] ... BEL or ST (ESC \) */
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
         p++; /* strip C0/DEL (newline/tab preserved by is_hard_control) */
         continue;
      }
      if (w + 1 > maxlen)
      {
         truncated = 1;
         break;
      }
      out[w++] = (char)c;
      p++;
   }
   out[w] = '\0';

   /* Pass 2: defang markers in place (replacement shrinks, so w_out <= r). */
   size_t r = 0, wr = 0;
   while (r < w)
   {
      size_t L = marker_at(&out[r]); /* out is NUL-terminated at w */
      if (L > 0 && r + L <= w)
      {
         out[wr++] = ' ';
         r += L;
      }
      else
         out[wr++] = out[r++];
   }
   size_t n = wr;
   if (truncated)
      utf8_trim(out, &n); /* a truncation may have split the final char */
   out[n] = '\0';
   if (truncated)
   {
      *reason = SANITIZE_REASON_LENGTH;
      return SANITIZE_TRUNCATED;
   }
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
