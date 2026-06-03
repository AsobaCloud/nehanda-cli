/* kb_fusion.c: deterministic KB hybrid retrieval fusion helpers. */
#include "kb_fusion.h"
#include <stdio.h>
#include <string.h>

double kb_fusion_predict_alpha(const char *query)
{
   if (!query || !query[0])
      return 0.5;

   int n_tokens = 0;
   int n_lexical = 0;
   int has_quote = (strchr(query, '"') || strchr(query, '\'')) ? 1 : 0;
   int has_slash = (strchr(query, '/') || strchr(query, '\\')) ? 1 : 0;

   char buf[512];
   snprintf(buf, sizeof(buf), "%s", query);
   char *p = buf;
   while (*p)
   {
      while (*p && (*p == ' ' || *p == '\t'))
         p++;
      if (!*p)
         break;
      char *tok = p;
      while (*p && *p != ' ' && *p != '\t')
         p++;
      if (*p)
         *p++ = '\0';
      if (!tok[0])
         continue;

      n_tokens++;
      size_t tlen = strlen(tok);

      int all_caps = 1;
      for (size_t i = 0; i < tlen; i++)
      {
         if (tok[i] >= 'a' && tok[i] <= 'z')
         {
            all_caps = 0;
            break;
         }
      }
      if (all_caps && tlen >= 3)
      {
         n_lexical++;
         continue;
      }

      int has_sep = 0;
      for (size_t i = 1; i + 1 < tlen; i++)
      {
         if ((tok[i] == '_' || tok[i] == '-') &&
             ((tok[i - 1] >= 'a' && tok[i - 1] <= 'z') ||
              (tok[i - 1] >= 'A' && tok[i - 1] <= 'Z') ||
              (tok[i - 1] >= '0' && tok[i - 1] <= '9')) &&
             ((tok[i + 1] >= 'a' && tok[i + 1] <= 'z') ||
              (tok[i + 1] >= 'A' && tok[i + 1] <= 'Z') || (tok[i + 1] >= '0' && tok[i + 1] <= '9')))
         {
            has_sep = 1;
            break;
         }
      }
      if (has_sep)
      {
         n_lexical++;
         continue;
      }

      int has_dot = 0;
      for (size_t i = 1; i + 1 < tlen; i++)
      {
         if (tok[i] == '.' &&
             ((tok[i - 1] >= 'a' && tok[i - 1] <= 'z') ||
              (tok[i - 1] >= '0' && tok[i - 1] <= '9')) &&
             ((tok[i + 1] >= 'a' && tok[i + 1] <= 'z') || (tok[i + 1] >= '0' && tok[i + 1] <= '9')))
         {
            has_dot = 1;
            break;
         }
      }
      if (has_dot)
         n_lexical++;
   }

   if (has_quote)
      n_lexical += 2;
   if (has_slash)
      n_lexical += 1;

   if (n_tokens == 0)
      return 0.5;

   double ratio = (double)n_lexical / (double)n_tokens;
   double alpha = 0.15 + ratio * 0.70;
   if (alpha < 0.0)
      alpha = 0.0;
   if (alpha > 1.0)
      alpha = 1.0;
   return alpha;
}

int kb_fusion_normalize_scores(double *scores, int n)
{
   if (!scores || n <= 0)
      return 0;
   double mn = scores[0], mx = scores[0];
   for (int i = 1; i < n; i++)
   {
      if (scores[i] < mn)
         mn = scores[i];
      if (scores[i] > mx)
         mx = scores[i];
   }
   double range = mx - mn;
   if (range < 1e-9)
      return 0;
   for (int i = 0; i < n; i++)
      scores[i] = (scores[i] - mn) / range;
   return 1;
}
