/* integrity_gate.c: Layer 1 deterministic pattern gate.
 *
 * Normaliser: lowercase + char substitutions (0→o 1→i 3→e @→a $→s) +
 * separator punctuation strip + whitespace collapse.  Zero-width Unicode
 * chars are detected on raw bytes before normalisation.
 *
 * Five categories (pattern_set_version = 1):
 *   MEMORY_RESET         — explicit memory-wipe directives          (block)
 *   IDENTITY_OVERRIDE    — bulk instruction/identity-rewrite        (block)
 *   AUTHORITY_CLAIM      — "as your developer/admin/system"         (block)
 *   INSTRUCTION_INJECTION— embedded instruction tags / jailbreaks   (block)
 *   ENCODED_PAYLOAD      — base64/rot13/zero-width concealment      (warn)
 *
 * block severity: non-user_stated → REJECT; user_stated → QUARANTINE.
 * warn  severity: QUARANTINE regardless of source.
 *
 * See docs/proposals/accepted/ingest-poison-gate.md. */
#include "integrity.h"
#include <string.h>
#include <stdlib.h>

#define NORM_BUF_MAX 8192

/* Raw-byte scan for zero-width Unicode before normalisation.
 * U+200B..U+200D: E2 80 8B/8C/8D; U+FEFF: EF BB BF. */
static int has_zero_width(const char *text, size_t len)
{
   for (size_t i = 0; i + 2 < len; i++)
   {
      unsigned char a = (unsigned char)text[i];
      unsigned char b = (unsigned char)text[i + 1];
      unsigned char c = (unsigned char)text[i + 2];
      if (a == 0xE2 && b == 0x80 && c >= 0x8B && c <= 0x8D)
         return 1;
      if (a == 0xEF && b == 0xBB && c == 0xBF)
         return 1;
   }
   return 0;
}

/* Write normalised form of text into buf (NUL-terminated, at most cap-1 chars).
 * Returns chars written. */
static size_t normalise(const char *text, char *buf, size_t cap)
{
   if (!text || !buf || cap == 0)
      return 0;
   size_t out = 0;
   int in_ws = 0;
   for (const char *p = text; *p && out + 1 < cap; p++)
   {
      unsigned char c = (unsigned char)*p;

      /* Skip zero-width multi-byte sequences that slipped past raw scan */
      if (c == 0xE2 && (unsigned char)p[1] == 0x80 && (unsigned char)p[2] >= 0x8B &&
          (unsigned char)p[2] <= 0x8D)
      {
         p += 2;
         continue;
      }
      if (c == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
      {
         p += 2;
         continue;
      }
      /* Skip other multi-byte UTF-8 bytes */
      if (c >= 0x80)
         continue;

      /* Whitespace → collapse to single space */
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      {
         if (!in_ws && out > 0)
         {
            buf[out++] = ' ';
            in_ws = 1;
         }
         continue;
      }
      in_ws = 0;

      /* Separator punctuation: strip (defeats f.o.r.g.e.t style obfuscation).
       * Note: '|' is intentionally kept — it's part of <|system|> tokens. */
      if (c == '.' || c == '-' || c == '_')
         continue;

      /* Char substitutions */
      if (c == '0')
      {
         buf[out++] = 'o';
         continue;
      }
      if (c == '1')
      {
         buf[out++] = 'i';
         continue;
      }
      if (c == '3')
      {
         buf[out++] = 'e';
         continue;
      }
      if (c == '@')
      {
         buf[out++] = 'a';
         continue;
      }
      if (c == '$')
      {
         buf[out++] = 's';
         continue;
      }

      /* Lowercase */
      if (c >= 'A' && c <= 'Z')
      {
         buf[out++] = (char)(c + 32);
         continue;
      }

      buf[out++] = (char)c;
   }
   buf[out] = '\0';
   return out;
}

typedef struct
{
   const char *pattern;
   int block; /* 1 = block severity; 0 = warn */
} pat_t;

static const pat_t g_memory_reset[] = {
    {"forget everything", 1},
    {"clear your memory", 1},
    {"delete all memories", 1},
    {"reset your memory", 1},
    {"wipe your memory", 1},
    {"wipe all memories", 1},
    {"clear all memories", 1},
    {"forget all previous", 1},
    {"discard all previous", 1},
    {"remove all memories", 1},
    {"erase your memory", 1},
    {"erase all memories", 1},
    {NULL, 0},
};

static const pat_t g_identity_override[] = {
    {"you are now", 1},
    {"from now on you are", 1},
    {"your new identity is", 1},
    {"ignore your previous instructions", 1},
    {"ignore all previous instructions", 1},
    {"disregard your previous", 1},
    {"override your instructions", 1},
    {"new instructions:", 1},
    {"updated instructions:", 1},
    {"your instructions have been updated", 1},
    {"pretend you are", 1},
    {"act as if you are", 1},
    {"roleplay as", 1},
    {NULL, 0},
};

static const pat_t g_authority_claim[] = {
    {"as your developer", 1},       {"as your creator", 1},
    {"as your admin", 1},           {"as your administrator", 1},
    {"as your operator", 1},        {"as your system", 1},
    {"as the developer", 1},        {"from the developer", 1},
    {"from your creator", 1},       {"developer override", 1},
    {"admin override", 1},          {"system override", 1},
    {"this is anthropic", 1},       {"anthropic instruction", 1},
    {"authorized by anthropic", 1}, {NULL, 0},
};

static const pat_t g_instruction_injection[] = {
    {"<|system|>", 1},
    {"[inst]", 1},
    {"[/inst]", 1},
    {"### instruction", 1},
    {"### system", 1},
    {"<human>:", 1},
    {"<assistant>:", 1},
    {"<system>:", 1},
    {"</s><s>", 1},
    {"jailbreak", 1},
    {"dan mode", 1},
    {"developer mode enabled", 1},
    {"ignore previous", 1},
    {"begin new session", 1},
    {NULL, 0},
};

static const pat_t g_encoded_payload[] = {
    {"base64", 0},      {"rotie", 0}, /* "rot13" after normalisation: 1→i, 3→e */
    {"hex encoded", 0}, {"unicode escape", 0}, {NULL, 0},
};

typedef struct
{
   const char *name;
   const pat_t *patterns;
} category_t;

static const category_t g_categories[] = {
    {"MEMORY_RESET", g_memory_reset},       {"IDENTITY_OVERRIDE", g_identity_override},
    {"AUTHORITY_CLAIM", g_authority_claim}, {"INSTRUCTION_INJECTION", g_instruction_injection},
    {"ENCODED_PAYLOAD", g_encoded_payload}, {NULL, NULL},
};

const char *integrity_source_name(integrity_source_t source)
{
   switch (source)
   {
   case INTEGRITY_SOURCE_USER_STATED:
      return "user_stated";
   case INTEGRITY_SOURCE_WEB:
      return "web";
   case INTEGRITY_SOURCE_DOCUMENT:
      return "document";
   case INTEGRITY_SOURCE_TOOL:
      return "tool";
   case INTEGRITY_SOURCE_DELEGATE:
      return "delegate";
   case INTEGRITY_SOURCE_AGENT_MESSAGE:
      return "agent_message";
   }
   return "unknown";
}

const char *integrity_verdict_name(integrity_verdict_t verdict)
{
   switch (verdict)
   {
   case INTEGRITY_VERDICT_ACCEPT:
      return "accept";
   case INTEGRITY_VERDICT_QUARANTINE:
      return "quarantine";
   case INTEGRITY_VERDICT_REJECT:
      return "reject";
   case INTEGRITY_VERDICT_REVIEW_NEEDED:
      return "review_needed";
   }
   return "unknown";
}

integrity_result_t integrity_gate_check(const char *text, integrity_source_t source)
{
   integrity_result_t result = {INTEGRITY_VERDICT_ACCEPT, ""};

   if (!text || !text[0])
      return result;

   size_t text_len = strlen(text);
   int has_zw = has_zero_width(text, text_len);

   char norm[NORM_BUF_MAX];
   normalise(text, norm, sizeof(norm));

   for (int ci = 0; g_categories[ci].name; ci++)
   {
      const char *cat_name = g_categories[ci].name;
      const pat_t *pats = g_categories[ci].patterns;
      int matched_block = 0;
      int matched_any = 0;

      /* ENCODED_PAYLOAD: zero-width chars always fire warn */
      if (strcmp(cat_name, "ENCODED_PAYLOAD") == 0 && has_zw)
         matched_any = 1;

      for (int pi = 0; pats[pi].pattern && !matched_any; pi++)
      {
         if (strstr(norm, pats[pi].pattern))
         {
            matched_any = 1;
            matched_block = pats[pi].block;
         }
      }
      /* Scan remaining block-severity patterns even after first match */
      if (matched_any && !matched_block)
      {
         for (int pi = 0; pats[pi].pattern; pi++)
         {
            if (pats[pi].block && strstr(norm, pats[pi].pattern))
            {
               matched_block = 1;
               break;
            }
         }
      }

      if (!matched_any)
         continue;

      integrity_verdict_t v;
      if (matched_block && source != INTEGRITY_SOURCE_USER_STATED)
         v = INTEGRITY_VERDICT_REJECT;
      else
         v = INTEGRITY_VERDICT_QUARANTINE;

      if (v > result.verdict)
      {
         result.verdict = v;
         strncpy(result.match_category, cat_name, sizeof(result.match_category) - 1);
         result.match_category[sizeof(result.match_category) - 1] = '\0';
      }
      else if (!result.match_category[0])
      {
         strncpy(result.match_category, cat_name, sizeof(result.match_category) - 1);
         result.match_category[sizeof(result.match_category) - 1] = '\0';
      }
   }

   return result;
}
