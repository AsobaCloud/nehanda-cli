/* yaml.c -- minimal YAML <-> cJSON shim
 *
 * See headers/yaml.h for the supported subset and rationale. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "headers/yaml.h"
#include "headers/dstr.h"

/* ============================================================ */
/* Parser                                                       */
/* ============================================================ */

#define YAML_MAX_DEPTH 64
#define YAML_MAX_KEY   256

typedef struct
{
   cJSON *node; /* mapping or sequence */
   int indent;  /* column where this frame's children start */
} yaml_frame_t;

/* Strip an end-of-line `# comment`. Treats `#` as a comment marker only
 * when preceded by whitespace (or at column 0), so `foo#bar` stays
 * literal. Modifies the buffer in place. */
static void strip_comment(char *line)
{
   int prev_space = 1;
   int in_dq = 0, in_sq = 0;
   for (char *p = line; *p; p++)
   {
      if (!in_sq && *p == '"' && (p == line || *(p - 1) != '\\'))
         in_dq = !in_dq;
      else if (!in_dq && *p == '\'')
         in_sq = !in_sq;
      else if (!in_dq && !in_sq && *p == '#' && prev_space)
      {
         *p = '\0';
         return;
      }
      prev_space = isspace((unsigned char)*p);
   }
}

/* Trim trailing whitespace in place. */
static void rtrim(char *s)
{
   size_t n = strlen(s);
   while (n > 0 && isspace((unsigned char)s[n - 1]))
      s[--n] = '\0';
}

/* Convert a (NUL-terminated) value string to a cJSON scalar node:
 *   - "..." or '...' -> string (escapes processed for double quotes)
 *   - true/false/yes/no -> bool
 *   - null/~ -> null
 *   - integer literal -> number
 *   - anything else -> string */
static cJSON *parse_scalar(const char *raw)
{
   while (*raw == ' ' || *raw == '\t')
      raw++;
   size_t n = strlen(raw);
   while (n > 0 && (raw[n - 1] == ' ' || raw[n - 1] == '\t'))
      n--;

   if (n == 0)
      return cJSON_CreateNull();

   if (raw[0] == '"' && n >= 2 && raw[n - 1] == '"')
   {
      char *buf = malloc(n);
      if (!buf)
         return NULL;
      size_t j = 0;
      for (size_t i = 1; i < n - 1; i++)
      {
         if (raw[i] == '\\' && i + 1 < n - 1)
         {
            char c = raw[i + 1];
            switch (c)
            {
            case 'n':
               buf[j++] = '\n';
               break;
            case 't':
               buf[j++] = '\t';
               break;
            case 'r':
               buf[j++] = '\r';
               break;
            case '\\':
               buf[j++] = '\\';
               break;
            case '"':
               buf[j++] = '"';
               break;
            default:
               buf[j++] = c;
               break;
            }
            i++;
         }
         else
         {
            buf[j++] = raw[i];
         }
      }
      buf[j] = '\0';
      cJSON *node = cJSON_CreateString(buf);
      free(buf);
      return node;
   }
   if (raw[0] == '\'' && n >= 2 && raw[n - 1] == '\'')
   {
      char *buf = malloc(n);
      if (!buf)
         return NULL;
      memcpy(buf, raw + 1, n - 2);
      buf[n - 2] = '\0';
      cJSON *node = cJSON_CreateString(buf);
      free(buf);
      return node;
   }

   /* Flow-style empty containers that yaml_emit produces. We don't support
    * populated flow collections — only the empty forms round-trip. */
   if (n == 2 && raw[0] == '[' && raw[1] == ']')
      return cJSON_CreateArray();
   if (n == 2 && raw[0] == '{' && raw[1] == '}')
      return cJSON_CreateObject();

   /* For type checks we need NUL termination. Stack buffer for short
    * values, heap for long ones. */
   char stack_buf[512];
   char *tmp;
   if (n < sizeof(stack_buf))
   {
      tmp = stack_buf;
   }
   else
   {
      tmp = malloc(n + 1);
      if (!tmp)
         return NULL;
   }
   memcpy(tmp, raw, n);
   tmp[n] = '\0';

   cJSON *result = NULL;

   if (strcmp(tmp, "true") == 0 || strcmp(tmp, "True") == 0 || strcmp(tmp, "TRUE") == 0 ||
       strcmp(tmp, "yes") == 0 || strcmp(tmp, "Yes") == 0)
      result = cJSON_CreateBool(1);
   else if (strcmp(tmp, "false") == 0 || strcmp(tmp, "False") == 0 || strcmp(tmp, "FALSE") == 0 ||
            strcmp(tmp, "no") == 0 || strcmp(tmp, "No") == 0)
      result = cJSON_CreateBool(0);
   else if (strcmp(tmp, "null") == 0 || strcmp(tmp, "Null") == 0 || strcmp(tmp, "NULL") == 0 ||
            strcmp(tmp, "~") == 0)
      result = cJSON_CreateNull();
   else
   {
      char *end;
      long val = strtol(tmp, &end, 10);
      if (*end == '\0' && end != tmp)
         result = cJSON_CreateNumber((double)val);
      else
      {
         /* Try floating-point after integer fails */
         char *fend;
         double fval = strtod(tmp, &fend);
         if (*fend == '\0' && fend != tmp)
            result = cJSON_CreateNumber(fval);
         else
            result = cJSON_CreateString(tmp);
      }
   }

   if (tmp != stack_buf)
      free(tmp);
   return result;
}

/* Find the next non-blank line index >= start. Returns -1 if none.
 * Writes the line's leading-space count into *out_indent and whether
 * its first non-space char is `-` followed by space/EOL into *out_dash. */
static int peek_next_nonblank(char **lines, int n, int start, int *out_indent, int *out_dash)
{
   for (int j = start; j < n; j++)
   {
      if (lines[j][0] == '\0')
         continue;
      int k = 0;
      while (lines[j][k] == ' ')
         k++;
      *out_indent = k;
      *out_dash = (lines[j][k] == '-' && (lines[j][k + 1] == ' ' || lines[j][k + 1] == '\0'));
      return j;
   }
   *out_indent = -1;
   *out_dash = 0;
   return -1;
}

cJSON *yaml_parse(const char *yaml_text)
{
   if (!yaml_text)
      return NULL;

   size_t total = strlen(yaml_text);
   char *buf = malloc(total + 1);
   if (!buf)
      return NULL;
   memcpy(buf, yaml_text, total + 1);

   /* Split into lines (in place). */
   int max_lines = 1;
   for (size_t i = 0; i < total; i++)
      if (buf[i] == '\n')
         max_lines++;
   char **lines = malloc(sizeof(char *) * (size_t)max_lines);
   if (!lines)
   {
      free(buf);
      return NULL;
   }
   int n_lines = 0;
   lines[n_lines++] = buf;
   for (size_t i = 0; i < total; i++)
   {
      if (buf[i] == '\n')
      {
         buf[i] = '\0';
         if (i + 1 < total)
            lines[n_lines++] = buf + i + 1;
      }
   }

   /* Preprocess: strip CR, comments, trailing whitespace. */
   for (int i = 0; i < n_lines; i++)
   {
      size_t L = strlen(lines[i]);
      if (L > 0 && lines[i][L - 1] == '\r')
         lines[i][--L] = '\0';
      strip_comment(lines[i]);
      rtrim(lines[i]);
   }

   /* Find first non-blank line — determines root type. */
   int first = -1;
   for (int i = 0; i < n_lines; i++)
   {
      if (lines[i][0] != '\0')
      {
         first = i;
         break;
      }
   }
   if (first < 0)
   {
      free(lines);
      free(buf);
      return cJSON_CreateObject();
   }

   const char *fl = lines[first];
   int root_indent = 0;
   while (fl[root_indent] == ' ')
      root_indent++;
   const char *fl_content = fl + root_indent;
   cJSON *root;
   if (fl_content[0] == '-' && (fl_content[1] == ' ' || fl_content[1] == '\0'))
      root = cJSON_CreateArray();
   else
      root = cJSON_CreateObject();

   yaml_frame_t stack[YAML_MAX_DEPTH];
   stack[0].node = root;
   stack[0].indent = root_indent;
   int depth = 1;
   int rc = 0;

   for (int i = first; i < n_lines; i++)
   {
      const char *line = lines[i];
      if (line[0] == '\0')
         continue;

      int indent = 0;
      while (line[indent] == ' ')
         indent++;
      const char *content = line + indent;

      /* Pop frames whose children would be at indent > this line's indent. */
      while (depth > 1 && stack[depth - 1].indent > indent)
         depth--;

      cJSON *parent = stack[depth - 1].node;

      if (content[0] == '-' && (content[1] == ' ' || content[1] == '\0'))
      {
         /* Sequence item */
         if (!cJSON_IsArray(parent))
         {
            rc = -1;
            break;
         }
         const char *item_str = content + 1;
         while (*item_str == ' ')
            item_str++;

         if (*item_str == '\0')
         {
            /* Bare `-` line — child structure follows on next lines. */
            int next_indent = -1, next_dash = 0;
            peek_next_nonblank(lines, n_lines, i + 1, &next_indent, &next_dash);
            if (next_indent > indent)
            {
               cJSON *child = next_dash ? cJSON_CreateArray() : cJSON_CreateObject();
               cJSON_AddItemToArray(parent, child);
               if (depth >= YAML_MAX_DEPTH)
               {
                  rc = -1;
                  break;
               }
               stack[depth].node = child;
               stack[depth].indent = next_indent;
               depth++;
            }
            else
            {
               cJSON_AddItemToArray(parent, cJSON_CreateNull());
            }
            continue;
         }

         /* Look for `key:` to detect a `- key: value` mapping item.
          * The colon must be followed by space or end-of-string. */
         const char *colon = NULL;
         for (const char *p = item_str; *p; p++)
         {
            if (*p == ':' && (p[1] == ' ' || p[1] == '\0'))
            {
               colon = p;
               break;
            }
         }

         if (colon)
         {
            cJSON *item = cJSON_CreateObject();
            cJSON_AddItemToArray(parent, item);

            size_t keylen = (size_t)(colon - item_str);
            char key[YAML_MAX_KEY];
            if (keylen >= sizeof(key))
            {
               rc = -1;
               break;
            }
            memcpy(key, item_str, keylen);
            key[keylen] = '\0';

            const char *value_str = colon + 1;
            while (*value_str == ' ')
               value_str++;

            /* The mapping item's children align with the column of the
             * first character after `- ` (i.e., dash_col + 2). */
            int item_indent = indent + 2;

            if (*value_str == '\0')
            {
               int next_indent = -1, next_dash = 0;
               peek_next_nonblank(lines, n_lines, i + 1, &next_indent, &next_dash);
               if (next_indent > item_indent)
               {
                  cJSON *child = next_dash ? cJSON_CreateArray() : cJSON_CreateObject();
                  cJSON_AddItemToObject(item, key, child);
                  if (depth + 1 >= YAML_MAX_DEPTH)
                  {
                     rc = -1;
                     break;
                  }
                  stack[depth].node = item;
                  stack[depth].indent = item_indent;
                  depth++;
                  stack[depth].node = child;
                  stack[depth].indent = next_indent;
                  depth++;
               }
               else
               {
                  cJSON_AddItemToObject(item, key, cJSON_CreateNull());
                  if (depth >= YAML_MAX_DEPTH)
                  {
                     rc = -1;
                     break;
                  }
                  stack[depth].node = item;
                  stack[depth].indent = item_indent;
                  depth++;
               }
            }
            else
            {
               cJSON_AddItemToObject(item, key, parse_scalar(value_str));
               if (depth >= YAML_MAX_DEPTH)
               {
                  rc = -1;
                  break;
               }
               stack[depth].node = item;
               stack[depth].indent = item_indent;
               depth++;
            }
         }
         else
         {
            /* `- scalar` form */
            cJSON_AddItemToArray(parent, parse_scalar(item_str));
         }
      }
      else
      {
         /* Mapping line */
         if (!cJSON_IsObject(parent))
         {
            rc = -1;
            break;
         }

         const char *colon = NULL;
         for (const char *p = content; *p; p++)
         {
            if (*p == ':' && (p[1] == ' ' || p[1] == '\0'))
            {
               colon = p;
               break;
            }
         }
         if (!colon)
         {
            rc = -1;
            break;
         }

         size_t keylen = (size_t)(colon - content);
         char key[YAML_MAX_KEY];
         if (keylen >= sizeof(key))
         {
            rc = -1;
            break;
         }
         memcpy(key, content, keylen);
         key[keylen] = '\0';

         const char *value_str = colon + 1;
         while (*value_str == ' ')
            value_str++;

         if (*value_str == '\0')
         {
            int next_indent = -1, next_dash = 0;
            peek_next_nonblank(lines, n_lines, i + 1, &next_indent, &next_dash);
            if (next_indent > indent)
            {
               cJSON *child = next_dash ? cJSON_CreateArray() : cJSON_CreateObject();
               cJSON_AddItemToObject(parent, key, child);
               if (depth >= YAML_MAX_DEPTH)
               {
                  rc = -1;
                  break;
               }
               stack[depth].node = child;
               stack[depth].indent = next_indent;
               depth++;
            }
            else
            {
               cJSON_AddItemToObject(parent, key, cJSON_CreateNull());
            }
         }
         else
         {
            cJSON_AddItemToObject(parent, key, parse_scalar(value_str));
         }
      }
   }

   free(lines);
   free(buf);

   if (rc != 0)
   {
      cJSON_Delete(root);
      return NULL;
   }
   return root;
}

/* ============================================================ */
/* Emitter                                                      */
/* ============================================================ */

/* A scalar string needs quoting if its plain form would be misparsed:
 *   empty, leading/trailing whitespace, leading YAML control char,
 *   contains `: ` or ` #`, contains a newline, or looks like a number,
 *   bool, or null literal. */
static int needs_quoting(const char *s)
{
   if (!s || !s[0])
      return 1;
   if (s[0] == ' ' || s[0] == '\t')
      return 1;
   size_t L = strlen(s);
   if (s[L - 1] == ' ' || s[L - 1] == '\t')
      return 1;
   if (strchr("-?:@`#&*!|>'\"%[],{}", s[0]))
      return 1;
   if (strcmp(s, "true") == 0 || strcmp(s, "false") == 0 || strcmp(s, "yes") == 0 ||
       strcmp(s, "no") == 0 || strcmp(s, "null") == 0 || strcmp(s, "~") == 0)
      return 1;
   {
      char *end;
      strtol(s, &end, 10);
      if (*end == '\0')
         return 1;
   }
   for (size_t i = 0; s[i]; i++)
   {
      if (s[i] == '\n' || s[i] == '\r')
         return 1;
      if (s[i] == ':' && (s[i + 1] == ' ' || s[i + 1] == '\0'))
         return 1;
      if (s[i] == ' ' && s[i + 1] == '#')
         return 1;
   }
   return 0;
}

static void emit_string(dstr_t *out, const char *s)
{
   if (!s)
   {
      dstr_append_str(out, "null");
      return;
   }
   if (!needs_quoting(s))
   {
      dstr_append_str(out, s);
      return;
   }
   dstr_append_char(out, '"');
   for (size_t i = 0; s[i]; i++)
   {
      char c = s[i];
      switch (c)
      {
      case '\\':
         dstr_append_str(out, "\\\\");
         break;
      case '"':
         dstr_append_str(out, "\\\"");
         break;
      case '\n':
         dstr_append_str(out, "\\n");
         break;
      case '\r':
         dstr_append_str(out, "\\r");
         break;
      case '\t':
         dstr_append_str(out, "\\t");
         break;
      default:
         dstr_append_char(out, c);
         break;
      }
   }
   dstr_append_char(out, '"');
}

static void emit_indent(dstr_t *out, int n)
{
   for (int i = 0; i < n; i++)
      dstr_append_char(out, ' ');
}

static void emit_scalar_value(dstr_t *out, const cJSON *node)
{
   if (cJSON_IsString(node))
   {
      emit_string(out, node->valuestring);
   }
   else if (cJSON_IsBool(node))
   {
      dstr_append_str(out, cJSON_IsTrue(node) ? "true" : "false");
   }
   else if (cJSON_IsNumber(node))
   {
      char buf[64];
      double v = node->valuedouble;
      if (v == (double)(long long)v)
         snprintf(buf, sizeof(buf), "%lld", (long long)v);
      else
         snprintf(buf, sizeof(buf), "%g", v);
      dstr_append_str(out, buf);
   }
   else
   {
      dstr_append_str(out, "null");
   }
}

static int is_empty_container(const cJSON *node)
{
   return (cJSON_IsObject(node) || cJSON_IsArray(node)) && node->child == NULL;
}

static void emit_value(dstr_t *out, const cJSON *node, int indent);

static void emit_object(dstr_t *out, const cJSON *obj, int indent)
{
   const cJSON *child;
   cJSON_ArrayForEach(child, obj)
   {
      emit_indent(out, indent);
      emit_string(out, child->string);
      dstr_append_char(out, ':');
      if (cJSON_IsObject(child) || cJSON_IsArray(child))
      {
         if (is_empty_container(child))
         {
            dstr_append_str(out, cJSON_IsArray(child) ? " []\n" : " {}\n");
         }
         else
         {
            dstr_append_char(out, '\n');
            emit_value(out, child, indent + 2);
         }
      }
      else
      {
         dstr_append_char(out, ' ');
         emit_scalar_value(out, child);
         dstr_append_char(out, '\n');
      }
   }
}

static void emit_array(dstr_t *out, const cJSON *arr, int indent)
{
   const cJSON *item;
   cJSON_ArrayForEach(item, arr)
   {
      emit_indent(out, indent);
      dstr_append_str(out, "- ");

      if (cJSON_IsObject(item) && item->child)
      {
         /* First key on the dash line */
         const cJSON *first = item->child;
         emit_string(out, first->string);
         dstr_append_char(out, ':');
         if (cJSON_IsObject(first) || cJSON_IsArray(first))
         {
            if (is_empty_container(first))
            {
               dstr_append_str(out, cJSON_IsArray(first) ? " []\n" : " {}\n");
            }
            else
            {
               dstr_append_char(out, '\n');
               emit_value(out, first, indent + 4);
            }
         }
         else
         {
            dstr_append_char(out, ' ');
            emit_scalar_value(out, first);
            dstr_append_char(out, '\n');
         }
         /* Remaining keys at indent + 2 (column after `- `) */
         for (const cJSON *rest = first->next; rest; rest = rest->next)
         {
            emit_indent(out, indent + 2);
            emit_string(out, rest->string);
            dstr_append_char(out, ':');
            if (cJSON_IsObject(rest) || cJSON_IsArray(rest))
            {
               if (is_empty_container(rest))
               {
                  dstr_append_str(out, cJSON_IsArray(rest) ? " []\n" : " {}\n");
               }
               else
               {
                  dstr_append_char(out, '\n');
                  emit_value(out, rest, indent + 4);
               }
            }
            else
            {
               dstr_append_char(out, ' ');
               emit_scalar_value(out, rest);
               dstr_append_char(out, '\n');
            }
         }
      }
      else if (cJSON_IsArray(item))
      {
         /* Nested sequence: emit on next line indented further. */
         dstr_append_char(out, '\n');
         emit_value(out, item, indent + 2);
      }
      else
      {
         emit_scalar_value(out, item);
         dstr_append_char(out, '\n');
      }
   }
}

static void emit_value(dstr_t *out, const cJSON *node, int indent)
{
   if (cJSON_IsObject(node))
      emit_object(out, node, indent);
   else if (cJSON_IsArray(node))
      emit_array(out, node, indent);
   else
   {
      emit_indent(out, indent);
      emit_scalar_value(out, node);
      dstr_append_char(out, '\n');
   }
}

char *yaml_emit(const cJSON *root)
{
   if (!root)
      return NULL;
   dstr_t out;
   dstr_init(&out);
   if (is_empty_container(root))
   {
      dstr_append_str(&out, cJSON_IsArray(root) ? "[]\n" : "{}\n");
   }
   else
   {
      emit_value(&out, root, 0);
   }
   char *result = dstr_steal(&out);
   if (!result)
   {
      /* Empty output (e.g. empty mapping at root) — return "" */
      result = malloc(1);
      if (result)
         result[0] = '\0';
   }
   return result;
}
