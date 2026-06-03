/* extractors_new_langs.c: language extractors for Java, Rust, Ruby, Kotlin, Swift, PHP */
#include "aimee.h"
#include "extractors_extra.h"
#include <ctype.h>

/* --- Java --- */

void java_import_line(const char *line, int lineno, void *ctx)
{
   import_ctx_t *ic = (import_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   if (strncmp(p, "import ", 7) != 0)
      return;
   p += 7;
   p = skip_ws(p);
   /* Skip static keyword */
   if (strncmp(p, "static ", 7) == 0)
      p += 7;

   char buf[512];
   size_t i = 0;
   while (p[i] && p[i] != ';' && !isspace((unsigned char)p[i]) && i < sizeof(buf) - 1)
   {
      buf[i] = p[i];
      i++;
   }
   buf[i] = '\0';
   if (i > 0)
      ic->count = add_str(ic->out, ic->count, ic->max, buf);
}

void java_export_line(const char *line, int lineno, void *ctx)
{
   export_ctx_t *ec = (export_ctx_t *)ctx;

   (void)lineno;

   if (!strstr(line, "public "))
      return;

   static const char *kw[] = {"class ", "interface ", "enum ", "record ", "@interface ", NULL};

   for (int i = 0; kw[i]; i++)
   {
      const char *p = strstr(line, kw[i]);
      if (p)
      {
         p += strlen(kw[i]);
         char name[256];
         if (extract_ident(p, name, sizeof(name)))
            ec->count = add_str(ec->out, ec->count, ec->max, name);
         return;
      }
   }
}

void java_route_line(const char *line, int lineno, void *ctx)
{
   export_ctx_t *rc = (export_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   static const char *attrs[] = {"@GetMapping(",
                                 "@PostMapping(",
                                 "@PutMapping(",
                                 "@DeleteMapping(",
                                 "@PatchMapping(",
                                 "@RequestMapping(",
                                 NULL};

   for (int i = 0; attrs[i]; i++)
   {
      if (strncmp(p, attrs[i], strlen(attrs[i])) == 0)
      {
         const char *q = p + strlen(attrs[i]);
         /* Skip optional value= */
         if (strncmp(q, "value = ", 8) == 0)
            q += 8;
         else if (strncmp(q, "value=", 6) == 0)
            q += 6;
         q = skip_ws(q);
         char buf[512];
         if (extract_quoted(q, buf, sizeof(buf)))
            rc->count = add_str(rc->out, rc->count, rc->max, buf);
         return;
      }
   }
}

void java_def_line(const char *line, int lineno, void *ctx)
{
   def_ctx_t *dc = (def_ctx_t *)ctx;
   const char *p = skip_ws(line);

   /* class/interface/enum/record */
   static const char *type_kw[] = {"class ", "interface ", "enum ", "record ", NULL};
   for (int i = 0; type_kw[i]; i++)
   {
      const char *kp = strstr(line, type_kw[i]);
      if (kp)
      {
         kp += strlen(type_kw[i]);
         char name[256];
         if (extract_ident(kp, name, sizeof(name)))
            dc->count = add_def(dc->out, dc->count, dc->max, name, "definition", lineno);
         return;
      }
   }

   /* Method: access modifiers + return type + name( */
   static const char *access[] = {"public ", "private ", "protected ", NULL};
   for (int i = 0; access[i]; i++)
   {
      if (strncmp(p, access[i], strlen(access[i])) == 0)
      {
         p += strlen(access[i]);
         /* Skip modifiers */
         while (strncmp(p, "static ", 7) == 0 || strncmp(p, "final ", 6) == 0 ||
                strncmp(p, "abstract ", 9) == 0 || strncmp(p, "synchronized ", 13) == 0 ||
                strncmp(p, "native ", 7) == 0 || strncmp(p, "default ", 8) == 0)
         {
            const char *sp = strchr(p, ' ');
            if (!sp)
               return;
            p = sp + 1;
         }
         /* Skip return type (including generics) */
         char dummy[256];
         if (!extract_ident(p, dummy, sizeof(dummy)))
            return;
         p += strlen(dummy);
         if (*p == '<')
         {
            /* Skip generic type parameter */
            int depth = 1;
            p++;
            while (*p && depth > 0)
            {
               if (*p == '<')
                  depth++;
               else if (*p == '>')
                  depth--;
               p++;
            }
         }
         /* Skip array brackets and spaces */
         while (*p == '[' || *p == ']' || *p == ' ')
            p++;
         char name[256];
         if (extract_ident(p, name, sizeof(name)))
         {
            p += strlen(name);
            if (*p == '(')
               dc->count = add_def(dc->out, dc->count, dc->max, name, "definition", lineno);
         }
         return;
      }
   }
}

/* --- Rust --- */

void rust_import_line(const char *line, int lineno, void *ctx)
{
   import_ctx_t *ic = (import_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   if (strncmp(p, "use ", 4) != 0)
      return;
   p += 4;
   p = skip_ws(p);

   char buf[512];
   size_t i = 0;
   while (p[i] && p[i] != ';' && p[i] != '{' && !isspace((unsigned char)p[i]) &&
          i < sizeof(buf) - 1)
   {
      buf[i] = p[i];
      i++;
   }
   /* Strip trailing :: */
   while (i >= 2 && buf[i - 1] == ':' && buf[i - 2] == ':')
      i -= 2;
   buf[i] = '\0';
   if (i > 0)
      ic->count = add_str(ic->out, ic->count, ic->max, buf);
}

void rust_export_line(const char *line, int lineno, void *ctx)
{
   export_ctx_t *ec = (export_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   /* pub fn / pub struct / pub trait / pub enum / pub type / pub const */
   if (strncmp(p, "pub ", 4) != 0)
      return;
   p += 4;
   if (strncmp(p, "async ", 6) == 0)
      p += 6;

   static const char *kw[] = {"fn ", "struct ", "trait ", "enum ", "type ", "const ", "mod ", NULL};
   for (int i = 0; kw[i]; i++)
   {
      if (strncmp(p, kw[i], strlen(kw[i])) == 0)
      {
         char name[256];
         if (extract_ident(p + strlen(kw[i]), name, sizeof(name)))
            ec->count = add_str(ec->out, ec->count, ec->max, name);
         return;
      }
   }
}

void rust_def_line(const char *line, int lineno, void *ctx)
{
   def_ctx_t *dc = (def_ctx_t *)ctx;
   const char *p = skip_ws(line);

   /* Skip pub/pub(crate)/pub(super) */
   if (strncmp(p, "pub", 3) == 0)
   {
      p += 3;
      if (*p == '(')
      {
         const char *close = strchr(p, ')');
         if (close)
            p = close + 1;
      }
      p = skip_ws(p);
   }

   if (strncmp(p, "async ", 6) == 0)
      p += 6;

   static const char *kw[] = {"fn ", "struct ", "trait ", "enum ", "type ", "impl ", "mod ", NULL};
   for (int i = 0; kw[i]; i++)
   {
      if (strncmp(p, kw[i], strlen(kw[i])) == 0)
      {
         const char *np = p + strlen(kw[i]);
         char name[256];
         if (extract_ident(np, name, sizeof(name)))
            dc->count = add_def(dc->out, dc->count, dc->max, name, "definition", lineno);
         return;
      }
   }
}

/* --- Ruby --- */

void ruby_import_line(const char *line, int lineno, void *ctx)
{
   import_ctx_t *ic = (import_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   if (strncmp(p, "require_relative ", 17) == 0)
   {
      p += 17;
      p = skip_ws(p);
      char buf[512];
      if (extract_quoted(p, buf, sizeof(buf)))
         ic->count = add_str(ic->out, ic->count, ic->max, buf);
      return;
   }

   if (strncmp(p, "require ", 8) == 0)
   {
      p += 8;
      p = skip_ws(p);
      char buf[512];
      if (extract_quoted(p, buf, sizeof(buf)))
         ic->count = add_str(ic->out, ic->count, ic->max, buf);
   }
}

void ruby_export_line(const char *line, int lineno, void *ctx)
{
   export_ctx_t *ec = (export_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   if (strncmp(p, "def ", 4) == 0)
   {
      /* Skip self. prefix */
      const char *np = p + 4;
      if (strncmp(np, "self.", 5) == 0)
         np += 5;
      char name[256];
      if (extract_ident(np, name, sizeof(name)))
         ec->count = add_str(ec->out, ec->count, ec->max, name);
      return;
   }

   static const char *kw[] = {"class ", "module ", NULL};
   for (int i = 0; kw[i]; i++)
   {
      if (strncmp(p, kw[i], strlen(kw[i])) == 0)
      {
         char name[256];
         if (extract_ident(p + strlen(kw[i]), name, sizeof(name)))
            ec->count = add_str(ec->out, ec->count, ec->max, name);
         return;
      }
   }
}

void ruby_route_line(const char *line, int lineno, void *ctx)
{
   export_ctx_t *rc = (export_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   static const char *methods[] = {"get ",   "post ",       "put ",       "delete ",
                                   "patch ", "resources :", "resource :", NULL};

   for (int i = 0; methods[i]; i++)
   {
      if (strncmp(p, methods[i], strlen(methods[i])) == 0)
      {
         const char *q = p + strlen(methods[i]);
         q = skip_ws(q);
         /* Either a quoted path or a symbol */
         char buf[512];
         if (*q == '\'' || *q == '"')
         {
            if (extract_quoted(q, buf, sizeof(buf)))
               rc->count = add_str(rc->out, rc->count, rc->max, buf);
         }
         else
         {
            /* resources :name */
            if (extract_ident(q, buf, sizeof(buf)))
               rc->count = add_str(rc->out, rc->count, rc->max, buf);
         }
         return;
      }
   }
}

void ruby_def_line(const char *line, int lineno, void *ctx)
{
   def_ctx_t *dc = (def_ctx_t *)ctx;
   const char *p = skip_ws(line);

   if (strncmp(p, "def ", 4) == 0)
   {
      const char *np = p + 4;
      if (strncmp(np, "self.", 5) == 0)
         np += 5;
      char name[256];
      if (extract_ident(np, name, sizeof(name)))
         dc->count = add_def(dc->out, dc->count, dc->max, name, "definition", lineno);
      return;
   }

   static const char *kw[] = {"class ", "module ", NULL};
   for (int i = 0; kw[i]; i++)
   {
      if (strncmp(p, kw[i], strlen(kw[i])) == 0)
      {
         char name[256];
         if (extract_ident(p + strlen(kw[i]), name, sizeof(name)))
            dc->count = add_def(dc->out, dc->count, dc->max, name, "definition", lineno);
         return;
      }
   }
}

/* --- Kotlin --- */

void kotlin_import_line(const char *line, int lineno, void *ctx)
{
   import_ctx_t *ic = (import_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   if (strncmp(p, "import ", 7) != 0)
      return;
   p += 7;
   p = skip_ws(p);

   char buf[512];
   size_t i = 0;
   while (p[i] && !isspace((unsigned char)p[i]) && i < sizeof(buf) - 1)
   {
      buf[i] = p[i];
      i++;
   }
   buf[i] = '\0';
   if (i > 0)
      ic->count = add_str(ic->out, ic->count, ic->max, buf);
}

void kotlin_export_line(const char *line, int lineno, void *ctx)
{
   export_ctx_t *ec = (export_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   /* Skip visibility modifiers */
   static const char *vis[] = {"public ", "internal ", "protected ", NULL};
   for (int i = 0; vis[i]; i++)
   {
      if (strncmp(p, vis[i], strlen(vis[i])) == 0)
      {
         p += strlen(vis[i]);
         break;
      }
   }

   static const char *kw[] = {"fun ",          "class ",          "data class ",
                              "interface ",    "object ",         "enum class ",
                              "sealed class ", "abstract class ", NULL};

   for (int i = 0; kw[i]; i++)
   {
      if (strncmp(p, kw[i], strlen(kw[i])) == 0)
      {
         char name[256];
         if (extract_ident(p + strlen(kw[i]), name, sizeof(name)))
            ec->count = add_str(ec->out, ec->count, ec->max, name);
         return;
      }
   }
}

void kotlin_route_line(const char *line, int lineno, void *ctx)
{
   export_ctx_t *rc = (export_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   /* Spring/Ktor style annotations */
   static const char *attrs[] = {"@GetMapping(",
                                 "@PostMapping(",
                                 "@PutMapping(",
                                 "@DeleteMapping(",
                                 "@PatchMapping(",
                                 "@RequestMapping(",
                                 NULL};

   for (int i = 0; attrs[i]; i++)
   {
      if (strncmp(p, attrs[i], strlen(attrs[i])) == 0)
      {
         const char *q = p + strlen(attrs[i]);
         if (strncmp(q, "value = ", 8) == 0)
            q += 8;
         else if (strncmp(q, "value=", 6) == 0)
            q += 6;
         q = skip_ws(q);
         char buf[512];
         if (extract_quoted(q, buf, sizeof(buf)))
            rc->count = add_str(rc->out, rc->count, rc->max, buf);
         return;
      }
   }
}

void kotlin_def_line(const char *line, int lineno, void *ctx)
{
   def_ctx_t *dc = (def_ctx_t *)ctx;
   const char *p = skip_ws(line);

   /* Skip visibility/modifiers */
   static const char *mods[] = {"public ",   "private ", "protected ", "internal ", "override ",
                                "abstract ", "open ",    "inline ",    "suspend ",  NULL};
   int changed = 1;
   while (changed)
   {
      changed = 0;
      for (int i = 0; mods[i]; i++)
      {
         if (strncmp(p, mods[i], strlen(mods[i])) == 0)
         {
            p += strlen(mods[i]);
            changed = 1;
            break;
         }
      }
   }

   static const char *kw[] = {"fun ",          "class ",          "data class ",
                              "interface ",    "object ",         "enum class ",
                              "sealed class ", "abstract class ", NULL};

   for (int i = 0; kw[i]; i++)
   {
      if (strncmp(p, kw[i], strlen(kw[i])) == 0)
      {
         char name[256];
         if (extract_ident(p + strlen(kw[i]), name, sizeof(name)))
            dc->count = add_def(dc->out, dc->count, dc->max, name, "definition", lineno);
         return;
      }
   }
}

/* --- Swift --- */

void swift_import_line(const char *line, int lineno, void *ctx)
{
   import_ctx_t *ic = (import_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   if (strncmp(p, "import ", 7) != 0)
      return;
   p += 7;
   p = skip_ws(p);

   char buf[512];
   size_t i = 0;
   while (p[i] && !isspace((unsigned char)p[i]) && i < sizeof(buf) - 1)
   {
      buf[i] = p[i];
      i++;
   }
   buf[i] = '\0';
   if (i > 0)
      ic->count = add_str(ic->out, ic->count, ic->max, buf);
}

void swift_export_line(const char *line, int lineno, void *ctx)
{
   export_ctx_t *ec = (export_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   /* Skip access modifiers */
   if (strncmp(p, "public ", 7) == 0 || strncmp(p, "open ", 5) == 0 ||
       strncmp(p, "internal ", 9) == 0)
   {
      const char *sp = strchr(p, ' ');
      if (sp)
         p = skip_ws(sp);
   }

   static const char *kw[] = {"func ", "class ", "struct ",    "protocol ",
                              "enum ", "actor ", "extension ", NULL};

   for (int i = 0; kw[i]; i++)
   {
      if (strncmp(p, kw[i], strlen(kw[i])) == 0)
      {
         char name[256];
         if (extract_ident(p + strlen(kw[i]), name, sizeof(name)))
            ec->count = add_str(ec->out, ec->count, ec->max, name);
         return;
      }
   }
}

void swift_def_line(const char *line, int lineno, void *ctx)
{
   def_ctx_t *dc = (def_ctx_t *)ctx;
   const char *p = skip_ws(line);

   /* Skip access/mutation modifiers */
   static const char *mods[] = {"public ",      "private ",  "internal ",    "open ",
                                "fileprivate ", "static ",   "class ",       "override ",
                                "final ",       "mutating ", "nonmutating ", NULL};
   int changed = 1;
   while (changed)
   {
      changed = 0;
      for (int i = 0; mods[i]; i++)
      {
         if (strncmp(p, mods[i], strlen(mods[i])) == 0)
         {
            p += strlen(mods[i]);
            changed = 1;
            break;
         }
      }
   }

   static const char *kw[] = {"func ",  "struct ",    "protocol ", "enum ",
                              "actor ", "extension ", NULL};

   for (int i = 0; kw[i]; i++)
   {
      if (strncmp(p, kw[i], strlen(kw[i])) == 0)
      {
         char name[256];
         if (extract_ident(p + strlen(kw[i]), name, sizeof(name)))
            dc->count = add_def(dc->out, dc->count, dc->max, name, "definition", lineno);
         return;
      }
   }

   /* class Name (not already consumed as a modifier) */
   if (strncmp(p, "class ", 6) == 0)
   {
      char name[256];
      if (extract_ident(p + 6, name, sizeof(name)))
         dc->count = add_def(dc->out, dc->count, dc->max, name, "definition", lineno);
   }
}

/* --- PHP --- */

void php_import_line(const char *line, int lineno, void *ctx)
{
   import_ctx_t *ic = (import_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   static const char *kw[] = {"require_once ", "require ", "include_once ", "include ", NULL};
   for (int i = 0; kw[i]; i++)
   {
      if (strncmp(p, kw[i], strlen(kw[i])) == 0)
      {
         p += strlen(kw[i]);
         p = skip_ws(p);
         /* Strip optional parentheses */
         if (*p == '(')
            p++;
         p = skip_ws(p);
         char buf[512];
         if (extract_quoted(p, buf, sizeof(buf)))
            ic->count = add_str(ic->out, ic->count, ic->max, buf);
         return;
      }
   }

   /* use Foo\Bar; */
   if (strncmp(p, "use ", 4) == 0)
   {
      p += 4;
      char buf[512];
      size_t i = 0;
      while (p[i] && p[i] != ';' && !isspace((unsigned char)p[i]) && p[i] != '{' &&
             i < sizeof(buf) - 1)
      {
         buf[i] = p[i];
         i++;
      }
      buf[i] = '\0';
      if (i > 0)
         ic->count = add_str(ic->out, ic->count, ic->max, buf);
   }
}

void php_export_line(const char *line, int lineno, void *ctx)
{
   export_ctx_t *ec = (export_ctx_t *)ctx;
   const char *p = skip_ws(line);

   (void)lineno;

   /* function name( */
   if (strncmp(p, "function ", 9) == 0)
   {
      const char *np = p + 9;
      /* Skip & for reference return */
      if (*np == '&')
         np++;
      char name[256];
      if (extract_ident(np, name, sizeof(name)))
         ec->count = add_str(ec->out, ec->count, ec->max, name);
      return;
   }

   /* Skip access modifiers */
   static const char *mods[] = {"public ",   "protected ", "private ", "static ",
                                "abstract ", "final ",     NULL};
   int changed = 1;
   while (changed)
   {
      changed = 0;
      for (int i = 0; mods[i]; i++)
      {
         if (strncmp(p, mods[i], strlen(mods[i])) == 0)
         {
            p += strlen(mods[i]);
            changed = 1;
            break;
         }
      }
   }

   static const char *kw[] = {"class ", "interface ", "trait ", "enum ", NULL};
   for (int i = 0; kw[i]; i++)
   {
      if (strncmp(p, kw[i], strlen(kw[i])) == 0)
      {
         char name[256];
         if (extract_ident(p + strlen(kw[i]), name, sizeof(name)))
            ec->count = add_str(ec->out, ec->count, ec->max, name);
         return;
      }
   }
}

void php_def_line(const char *line, int lineno, void *ctx)
{
   def_ctx_t *dc = (def_ctx_t *)ctx;
   const char *p = skip_ws(line);

   /* function name( */
   if (strncmp(p, "function ", 9) == 0)
   {
      const char *np = p + 9;
      if (*np == '&')
         np++;
      char name[256];
      if (extract_ident(np, name, sizeof(name)))
         dc->count = add_def(dc->out, dc->count, dc->max, name, "definition", lineno);
      return;
   }

   /* Skip access modifiers */
   static const char *mods[] = {"public ",   "protected ", "private ", "static ",
                                "abstract ", "final ",     NULL};
   int changed = 1;
   while (changed)
   {
      changed = 0;
      for (int i = 0; mods[i]; i++)
      {
         if (strncmp(p, mods[i], strlen(mods[i])) == 0)
         {
            p += strlen(mods[i]);
            changed = 1;
            break;
         }
      }
   }

   static const char *kw[] = {"class ", "interface ", "trait ", "enum ", "function ", NULL};
   for (int i = 0; kw[i]; i++)
   {
      if (strncmp(p, kw[i], strlen(kw[i])) == 0)
      {
         char name[256];
         if (extract_ident(p + strlen(kw[i]), name, sizeof(name)))
            dc->count = add_def(dc->out, dc->count, dc->max, name, "definition", lineno);
         return;
      }
   }
}
