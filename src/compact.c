/* compact.c: tool-result compaction — JSON structural summaries, head/tail truncation.
 *
 * Three strategies, applied in order:
 *   1. Pass-through: content fits within the effective threshold.
 *   2. JSON summary: content parses as JSON → emit structural key/type outline.
 *   3. Head+tail: keep leading and trailing regions with a truncation notice.
 *
 * Dynamic threshold tightening reduces the allowed size as session context fills. */
#include "aimee.h"
#include "compact.h"
#include "cJSON.h"
#include <ctype.h>

/* ------------------------------------------------------------------ helpers */

static int starts_with_json(const char *s, size_t len)
{
   size_t i = 0;
   while (i < len && isspace((unsigned char)s[i]))
      i++;
   return i < len && (s[i] == '{' || s[i] == '[');
}

/* ------------------------------------------------------------------ JSON summary */

/* Recursively describe JSON structure into buf.
 * Depth-limited to avoid enormous summaries for deeply nested documents. */
static void describe_json(const cJSON *node, char *buf, size_t *pos, size_t cap, int depth)
{
   if (!node || *pos >= cap)
      return;

   int n;

#define APPEND(fmt, ...)                                                                           \
   do                                                                                              \
   {                                                                                               \
      n = snprintf(buf + *pos, cap - *pos, fmt, ##__VA_ARGS__);                                    \
      if (n > 0)                                                                                   \
         *pos += (size_t)n;                                                                        \
   } while (0)

   if (cJSON_IsObject(node))
   {
      int count = cJSON_GetArraySize(node);
      APPEND("{");
      int first = 1;
      const cJSON *child;
      cJSON_ArrayForEach(child, node)
      {
         if (*pos >= cap)
            break;
         if (!first)
            APPEND(", ");
         first = 0;
         APPEND("\"%s\": ", child->string ? child->string : "");
         if (depth < 2)
         {
            describe_json(child, buf, pos, cap, depth + 1);
         }
         else
         {
            const char *t = cJSON_IsString(child)   ? "<string>"
                            : cJSON_IsNumber(child) ? "<number>"
                            : cJSON_IsObject(child) ? "{...}"
                            : cJSON_IsArray(child)  ? "[...]"
                            : cJSON_IsBool(child)   ? "<bool>"
                                                    : "null";
            APPEND("%s", t);
         }
      }
      APPEND("}");
      if (count > 5)
         APPEND(" /* %d keys */", count);
   }
   else if (cJSON_IsArray(node))
   {
      int count = cJSON_GetArraySize(node);
      if (count == 0)
      {
         APPEND("[]");
      }
      else
      {
         APPEND("[/* %d items */", count);
         if (depth < 2)
         {
            APPEND(" ");
            describe_json(cJSON_GetArrayItem(node, 0), buf, pos, cap, depth + 1);
            if (count > 1)
               APPEND(", ...");
         }
         APPEND("]");
      }
   }
   else if (cJSON_IsString(node))
   {
      size_t vlen = node->valuestring ? strlen(node->valuestring) : 0;
      if (vlen <= 64)
         APPEND("\"%s\"", node->valuestring ? node->valuestring : "");
      else
         APPEND("\"%.60s...\"", node->valuestring);
   }
   else if (cJSON_IsNumber(node))
   {
      APPEND("%g", node->valuedouble);
   }
   else if (cJSON_IsBool(node))
   {
      APPEND("%s", cJSON_IsTrue(node) ? "true" : "false");
   }
   else
   {
      APPEND("null");
   }

#undef APPEND
}

static char *compact_json_summary(const char *raw, size_t raw_len)
{
   cJSON *parsed = cJSON_ParseWithLength(raw, raw_len);
   if (!parsed)
      return NULL; /* not valid JSON — caller falls back to plain-text strategy */

   char summary[2048];
   size_t pos = 0;
   int n = snprintf(summary, sizeof(summary), "[compacted JSON summary]\n");
   if (n > 0)
      pos = (size_t)n;

   describe_json(parsed, summary, &pos, sizeof(summary) - 64, 0);
   cJSON_Delete(parsed);

   /* Append size hint */
   n = snprintf(summary + pos, sizeof(summary) - pos, "\n[original: %zu bytes]", raw_len);
   if (n > 0)
      pos += (size_t)n;
   summary[pos < sizeof(summary) ? pos : sizeof(summary) - 1] = '\0';

   return strdup(summary);
}

/* ------------------------------------------------------------------ plain-text head+tail */

static char *compact_plaintext(const char *raw, size_t raw_len, int head_bytes, int tail_bytes)
{
   size_t head = (size_t)head_bytes;
   size_t tail = (size_t)tail_bytes;

   if (head + tail >= raw_len)
      return strdup(raw);

   size_t omitted = raw_len - head - tail;
   char notice[128];
   int n = snprintf(notice, sizeof(notice), "\n[... %zu bytes omitted ...]\n", omitted);
   size_t notice_len = (n > 0) ? (size_t)n : 0;

   size_t out_len = head + notice_len + tail;
   char *out = malloc(out_len + 1);
   if (!out)
   {
      /* fallback: return the tail only */
      char *fb = malloc(tail + 1);
      if (!fb)
         return strdup("");
      memcpy(fb, raw + raw_len - tail, tail);
      fb[tail] = '\0';
      return fb;
   }

   memcpy(out, raw, head);
   if (notice_len)
      memcpy(out + head, notice, notice_len);
   memcpy(out + head + notice_len, raw + raw_len - tail, tail);
   out[out_len] = '\0';
   return out;
}

/* ------------------------------------------------------------------ public API */

char *compact_tool_result(const char *raw, size_t raw_len, const compact_config_t *cfg,
                          const char *tool_name, int context_used, int context_budget)
{
   if (!raw)
      return strdup("");

   /* Resolve effective parameters from config (or defaults) */
   int enabled = 1;
   int threshold = COMPACT_DEFAULT_THRESHOLD;
   int head_bytes = COMPACT_DEFAULT_HEAD_BYTES;
   int tail_bytes = COMPACT_DEFAULT_TAIL_BYTES;

   if (cfg)
   {
      enabled = cfg->enabled;
      if (cfg->threshold > 0)
         threshold = cfg->threshold;
      if (cfg->head_bytes > 0)
         head_bytes = cfg->head_bytes;
      if (cfg->tail_bytes > 0)
         tail_bytes = cfg->tail_bytes;

      /* Per-tool override */
      if (tool_name)
      {
         for (int i = 0; i < cfg->per_tool_count; i++)
         {
            if (strcmp(cfg->per_tool[i].tool, tool_name) == 0)
            {
               if (cfg->per_tool[i].threshold == -1)
                  return strdup(raw); /* compaction disabled for this tool */
               threshold = cfg->per_tool[i].threshold;
               break;
            }
         }
      }
   }

   if (!enabled)
      return strdup(raw);

   /* Dynamic budget adjustment: shrink threshold as context fills */
   if (context_budget > 0 && context_used > 0)
   {
      int used_pct = (int)((long long)context_used * 100 / context_budget);
      if (used_pct >= 80)
         threshold = threshold / 2;
      else if (used_pct >= 60)
         threshold = threshold * 3 / 4;
   }

   if (threshold < 64)
      threshold = 64; /* floor: never compact below 64 bytes */

   if ((int)raw_len <= threshold)
      return strdup(raw);

   /* Strategy 1: JSON structural summary */
   if (starts_with_json(raw, raw_len))
   {
      char *summary = compact_json_summary(raw, raw_len);
      if (summary)
         return summary;
   }

   /* Strategy 2: plain-text head + tail */
   return compact_plaintext(raw, raw_len, head_bytes, tail_bytes);
}
