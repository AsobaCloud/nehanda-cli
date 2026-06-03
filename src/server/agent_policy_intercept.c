/* agent_policy_intercept.c: discovery-shell command classifier.
 * No heavy deps — pure string matching so this is independently testable. */
#include "headers/agent_policy_intercept.h"
#include <string.h>

/* Operational path prefixes that are always allowed regardless of command. */
static int targets_operational_path(const char *cmd)
{
   return strstr(cmd, "/var/") || strstr(cmd, "/tmp/") || strstr(cmd, "/proc/") ||
          strstr(cmd, "/sys/") || strstr(cmd, "/run/") || strstr(cmd, "/etc/");
}

static int is_space_char(char ch)
{
   return ch == ' ' || ch == '\t';
}

static int token_is_docs_path(const char *tok, size_t len)
{
   if (!tok || len == 0)
      return 0;

   if ((tok[0] == '\'' || tok[0] == '"') && len > 1)
   {
      tok++;
      len--;
      if (len > 0 && (tok[len - 1] == '\'' || tok[len - 1] == '"'))
         len--;
   }

   if (len == 4 && strncmp(tok, "docs", 4) == 0)
      return 1;
   if (len > 5 && strncmp(tok, "docs/", 5) == 0)
      return 1;
   if (len == 6 && strncmp(tok, "./docs", 6) == 0)
      return 1;
   if (len > 7 && strncmp(tok, "./docs/", 7) == 0)
      return 1;
   return 0;
}

static int targets_documentation_path(const char *cmd)
{
   const char *p = cmd;
   while (*p)
   {
      while (is_space_char(*p))
         p++;
      const char *start = p;
      while (*p && !is_space_char(*p))
         p++;
      if (token_is_docs_path(start, (size_t)(p - start)))
         return 1;
   }
   return 0;
}

static int token_looks_specific_file_path(const char *tok, size_t len)
{
   if (!tok || len == 0)
      return 0;
   if ((tok[0] == '\'' || tok[0] == '"') && len > 1)
   {
      tok++;
      len--;
      if (len > 0 && (tok[len - 1] == '\'' || tok[len - 1] == '"'))
         len--;
   }
   if (len == 0 || tok[0] == '-' || tok[len - 1] == '/' || memchr(tok, '*', len))
      return 0;
   if (len == 1 && tok[0] == '.')
      return 0;
   if (len == 3 && strncmp(tok, "src", 3) == 0)
      return 0;
   if (len >= 9 && strncmp(tok, "--include", 9) == 0)
      return 0;
   if (memchr(tok, '.', len))
      return 1;
   return (len == 6 && strncmp(tok, "README", 6) == 0) ||
          (len == 8 && strncmp(tok, "Makefile", 8) == 0);
}

static int command_targets_specific_file_path(const char *cmd)
{
   const char *p = cmd;
   int token_index = 0;
   int saw_pattern = 0;
   while (*p)
   {
      while (is_space_char(*p))
         p++;
      const char *start = p;
      while (*p && !is_space_char(*p))
         p++;
      size_t len = (size_t)(p - start);
      if (len == 0)
         continue;
      if (token_index++ == 0)
         continue;
      if (start[0] == '-')
         continue;
      if (!saw_pattern)
      {
         saw_pattern = 1;
         continue;
      }
      if (token_looks_specific_file_path(start, len))
         return 1;
   }
   return 0;
}

int policy_is_source_discovery(const char *cmd)
{
   if (!cmd)
      return 0;

   const char *c = cmd;
   while (*c == ' ' || *c == '\t')
      c++;

   /* grep / rg / ripgrep */
   int is_grep = (strncmp(c, "grep", 4) == 0 && (c[4] == ' ' || c[4] == '\t' || c[4] == '\0'));
   int is_rg = (strcmp(c, "rg") == 0 || strncmp(c, "rg ", 3) == 0 || strncmp(c, "rg\t", 3) == 0);
   int is_ripgrep =
       (strncmp(c, "ripgrep", 7) == 0 && (c[7] == ' ' || c[7] == '\t' || c[7] == '\0'));
   if (is_grep || is_rg || is_ripgrep)
   {
      if (command_targets_specific_file_path(c))
         return 0;
      return !targets_operational_path(c) && !targets_documentation_path(c);
   }

   /* find — block on relative paths or source-extension patterns */
   if (strncmp(c, "find ", 5) == 0 || strncmp(c, "find\t", 5) == 0)
   {
      if (targets_documentation_path(c))
         return 0;
      const char *fp = c + 5;
      while (*fp == ' ' || *fp == '\t')
         fp++;
      if (*fp != '/')
         return 1;
      if (strstr(c, "*.c\"") || strstr(c, "*.c'") || strstr(c, "*.h\"") || strstr(c, "*.h'") ||
          strstr(c, "*.py\"") || strstr(c, "*.py'") || strstr(c, "*.go\"") || strstr(c, "*.go'") ||
          strstr(c, "*.rs\"") || strstr(c, "*.rs'"))
         return 1;
      return 0;
   }

   return 0;
}
