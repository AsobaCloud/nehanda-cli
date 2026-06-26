/* code_treesitter.c: §2 tree-sitter extraction front-end. See code_treesitter.h.
 *
 * Real implementation under -DAIMEE_TREESITTER (links the fetched tree-sitter runtime +
 * grammars); otherwise a stub that reports "unavailable" so callers fall back to the
 * hand-rolled extractors and the default build needs no tree-sitter at all.
 *
 * Languages mirror the hand-rolled supported set (extractors.c): C, C++, C#, Python, Go,
 * JavaScript, TypeScript, Rust, Java, Ruby, PHP, Lua, Bash, Swift, Kotlin, Dart, CSS. Add
 * one by vendoring its grammar (scripts/fetch-treesitter.sh + src/Makefile), declaring its
 * entry point, mapping its extension(s) in ts_language_for_ext, adding a ts_lang_t, and a
 * classify_<lang> for its definition node types. */

#include "headers/code_treesitter.h"

#include <string.h>

#ifdef AIMEE_TREESITTER

#include "tree_sitter/api.h"
#include <stdio.h>

/* Grammar entry points (defined in the vendored parser.c files). */
const TSLanguage *tree_sitter_c(void);
const TSLanguage *tree_sitter_cpp(void);
const TSLanguage *tree_sitter_c_sharp(void);
const TSLanguage *tree_sitter_python(void);
const TSLanguage *tree_sitter_go(void);
const TSLanguage *tree_sitter_javascript(void);
const TSLanguage *tree_sitter_typescript(void);
const TSLanguage *tree_sitter_tsx(void);
const TSLanguage *tree_sitter_rust(void);
const TSLanguage *tree_sitter_java(void);
const TSLanguage *tree_sitter_ruby(void);
const TSLanguage *tree_sitter_php(void);
const TSLanguage *tree_sitter_lua(void);
const TSLanguage *tree_sitter_bash(void);
const TSLanguage *tree_sitter_swift(void);
const TSLanguage *tree_sitter_kotlin(void);
const TSLanguage *tree_sitter_dart(void);
const TSLanguage *tree_sitter_css(void);

typedef enum
{
   TSL_C,
   TSL_CPP,
   TSL_CSHARP,
   TSL_PY,
   TSL_GO,
   TSL_JS,
   TSL_TS,
   TSL_RUST,
   TSL_JAVA,
   TSL_RUBY,
   TSL_PHP,
   TSL_LUA,
   TSL_BASH,
   TSL_SWIFT,
   TSL_KOTLIN,
   TSL_DART,
   TSL_CSS
} ts_lang_t;

static const TSLanguage *ts_language_for_ext(const char *ext, ts_lang_t *which)
{
   if (!ext)
      return NULL;
   static const struct
   {
      const char *ext;
      ts_lang_t lang;
      const TSLanguage *(*fn)(void);
   } map[] = {
       {".c", TSL_C, tree_sitter_c},
       {".h", TSL_C, tree_sitter_c},
       {".cpp", TSL_CPP, tree_sitter_cpp},
       {".cc", TSL_CPP, tree_sitter_cpp},
       {".cxx", TSL_CPP, tree_sitter_cpp},
       {".hpp", TSL_CPP, tree_sitter_cpp},
       {".hh", TSL_CPP, tree_sitter_cpp},
       {".cs", TSL_CSHARP, tree_sitter_c_sharp},
       {".py", TSL_PY, tree_sitter_python},
       {".go", TSL_GO, tree_sitter_go},
       {".js", TSL_JS, tree_sitter_javascript},
       {".mjs", TSL_JS, tree_sitter_javascript},
       {".cjs", TSL_JS, tree_sitter_javascript},
       {".jsx", TSL_JS, tree_sitter_javascript},
       {".ts", TSL_TS, tree_sitter_typescript},
       {".mts", TSL_TS, tree_sitter_typescript},
       {".cts", TSL_TS, tree_sitter_typescript},
       {".tsx", TSL_TS, tree_sitter_tsx},
       {".rs", TSL_RUST, tree_sitter_rust},
       {".java", TSL_JAVA, tree_sitter_java},
       {".rb", TSL_RUBY, tree_sitter_ruby},
       {".php", TSL_PHP, tree_sitter_php},
       {".lua", TSL_LUA, tree_sitter_lua},
       {".sh", TSL_BASH, tree_sitter_bash},
       {".bash", TSL_BASH, tree_sitter_bash},
       {".zsh", TSL_BASH, tree_sitter_bash},
       {".swift", TSL_SWIFT, tree_sitter_swift},
       {".kt", TSL_KOTLIN, tree_sitter_kotlin},
       {".kts", TSL_KOTLIN, tree_sitter_kotlin},
       {".dart", TSL_DART, tree_sitter_dart},
       {".css", TSL_CSS, tree_sitter_css},
   };
   for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); i++)
      if (strcmp(ext, map[i].ext) == 0)
      {
         if (which)
            *which = map[i].lang;
         return map[i].fn();
      }
   return NULL;
}

int code_treesitter_available(const char *ext)
{
   return ts_language_for_ext(ext, NULL) != NULL;
}

/* Copy a node's source span into out[cap] (NUL-terminated, truncated to fit). */
static void node_text(TSNode node, const char *src, char *out, int cap)
{
   uint32_t s = ts_node_start_byte(node), e = ts_node_end_byte(node);
   int n = (int)(e - s);
   if (n < 0)
      n = 0;
   if (n > cap - 1)
      n = cap - 1;
   memcpy(out, src + s, (size_t)n);
   out[n] = '\0';
}

/* Pre-order DFS for the first descendant whose type is one of the identifier kinds —
 * used to dig a name out of a subtree (e.g. the function name under a declarator, or a
 * Kotlin definition that exposes no `name` field). */
static int first_identifier(TSNode node, TSNode *out)
{
   const char *t = ts_node_type(node);
   if (strcmp(t, "identifier") == 0 || strcmp(t, "type_identifier") == 0 ||
       strcmp(t, "field_identifier") == 0 || strcmp(t, "simple_identifier") == 0 ||
       strcmp(t, "constant") == 0 || strcmp(t, "name") == 0)
   {
      *out = node;
      return 1;
   }
   uint32_t n = ts_node_named_child_count(node);
   for (uint32_t i = 0; i < n; i++)
      if (first_identifier(ts_node_named_child(node, i), out))
         return 1;
   return 0;
}

/* The name node for a definition: its `name` field if it has one, else the first
 * identifier-like descendant. Returns a possibly-null node (caller re-checks). */
static TSNode name_node(TSNode node)
{
   TSNode nm = ts_node_child_by_field_name(node, "name", 4);
   if (!ts_node_is_null(nm))
      return nm;
   TSNode id;
   if (first_identifier(node, &id))
      return id;
   return nm; /* null */
}

/* Find a direct named child of a given type; returns 1 and sets *out on a match. */
static int child_of_type(TSNode node, const char *type, TSNode *out)
{
   uint32_t n = ts_node_named_child_count(node);
   for (uint32_t i = 0; i < n; i++)
   {
      TSNode c = ts_node_named_child(node, i);
      if (strcmp(ts_node_type(c), type) == 0)
      {
         *out = c;
         return 1;
      }
   }
   return 0;
}

/* Map a node type to a kind via two NULL-terminated type lists; sets *kind and returns 1
 * on a match. Used by the languages whose definition name lives in a `name` field. */
static int kind_lookup(const char *t, const char *const *fns, const char *const *types,
                       const char **kind)
{
   for (int i = 0; fns[i]; i++)
      if (strcmp(t, fns[i]) == 0)
      {
         *kind = "function";
         return 1;
      }
   for (int i = 0; types[i]; i++)
      if (strcmp(t, types[i]) == 0)
      {
         *kind = "type";
         return 1;
      }
   return 0;
}

/* C: the function name lives in the declarator (survives pointer/array wrapping), so dig
 * it out explicitly rather than via name_node (which would grab a named return type). */
static int classify_c(TSNode node, const char **kind, TSNode *name_root)
{
   const char *t = ts_node_type(node);
   if (strcmp(t, "function_definition") == 0)
   {
      TSNode d = ts_node_child_by_field_name(node, "declarator", 10);
      if (ts_node_is_null(d) || !first_identifier(d, name_root))
         return 0;
      *kind = "function";
      return 1;
   }
   if (strcmp(t, "declaration") == 0)
   {
      /* a top-level function prototype: declarator is a function_declarator. */
      TSNode d = ts_node_child_by_field_name(node, "declarator", 10);
      if (!ts_node_is_null(d) && strstr(ts_node_type(d), "function_declarator") &&
          first_identifier(d, name_root))
      {
         *kind = "function";
         return 1;
      }
      return 0;
   }
   if (strcmp(t, "type_definition") == 0)
   {
      TSNode d = ts_node_child_by_field_name(node, "declarator", 10);
      if (ts_node_is_null(d) || !first_identifier(d, name_root))
         return 0;
      *kind = "type"; /* typedef: the new name is the declarator */
      return 1;
   }
   if (strcmp(t, "struct_specifier") == 0 || strcmp(t, "union_specifier") == 0 ||
       strcmp(t, "enum_specifier") == 0)
   {
      TSNode nm = ts_node_child_by_field_name(node, "name", 4);
      if (ts_node_is_null(nm))
         return 0; /* anonymous */
      *kind = "type";
      *name_root = nm;
      return 1;
   }
   return 0;
}

/* C++: C's rules plus class_specifier; namespaces/templates are containers (see below). */
static int classify_cpp(TSNode node, const char **kind, TSNode *name_root)
{
   if (classify_c(node, kind, name_root))
      return 1;
   if (strcmp(ts_node_type(node), "class_specifier") == 0)
   {
      TSNode nm = ts_node_child_by_field_name(node, "name", 4);
      if (ts_node_is_null(nm))
         return 0;
      *kind = "type";
      *name_root = nm;
      return 1;
   }
   return 0;
}

/* Python: function/class definitions, including those wrapped by a decorator. */
static int classify_py(TSNode node, const char **kind, TSNode *name_root)
{
   const char *t = ts_node_type(node);
   if (strcmp(t, "decorated_definition") == 0)
   {
      TSNode inner = ts_node_child_by_field_name(node, "definition", 10);
      if (ts_node_is_null(inner))
         return 0;
      return classify_py(inner, kind, name_root); /* inner is a function/class def */
   }
   if (strcmp(t, "function_definition") == 0)
      *kind = "function";
   else if (strcmp(t, "class_definition") == 0)
      *kind = "type";
   else
      return 0;
   *name_root = name_node(node);
   return 1;
}

/* Go: func/method declarations and named types (type_spec inside a type_declaration). */
static int classify_go(TSNode node, const char **kind, TSNode *name_root)
{
   const char *t = ts_node_type(node);
   if (strcmp(t, "function_declaration") == 0 || strcmp(t, "method_declaration") == 0)
   {
      *kind = "function";
      *name_root = name_node(node);
      return 1;
   }
   if (strcmp(t, "type_declaration") == 0)
   {
      uint32_t n = ts_node_named_child_count(node);
      for (uint32_t i = 0; i < n; i++)
      {
         TSNode spec = ts_node_named_child(node, i);
         const char *st = ts_node_type(spec);
         if (strcmp(st, "type_spec") == 0 || strcmp(st, "type_alias") == 0)
         {
            TSNode nm = ts_node_child_by_field_name(spec, "name", 4);
            if (!ts_node_is_null(nm))
            {
               *kind = "type";
               *name_root = nm;
               return 1;
            }
         }
      }
      return 0;
   }
   return 0;
}

/* JavaScript / TypeScript: function (incl. generator) and class/interface/type/enum
 * declarations, including those behind an `export`/`export default`. */
static int classify_jsts(TSNode node, const char **kind, TSNode *name_root)
{
   const char *t = ts_node_type(node);
   if (strcmp(t, "export_statement") == 0)
   {
      TSNode inner = ts_node_child_by_field_name(node, "declaration", 11);
      if (ts_node_is_null(inner))
         return 0; /* re-export / `export const x = ...` — no named declaration */
      return classify_jsts(inner, kind, name_root);
   }
   static const char *const F[] = {"function_declaration", "generator_function_declaration", NULL};
   static const char *const T[] = {"class_declaration", "interface_declaration",
                                   "type_alias_declaration", "enum_declaration", NULL};
   if (!kind_lookup(t, F, T, kind))
      return 0;
   *name_root = name_node(node);
   return 1;
}

/* Rust: functions and the named type-like items. */
static int classify_rust(TSNode node, const char **kind, TSNode *name_root)
{
   static const char *const F[] = {"function_item", NULL};
   static const char *const T[] = {"struct_item", "enum_item",  "trait_item",
                                   "type_item",   "union_item", NULL};
   if (!kind_lookup(ts_node_type(node), F, T, kind))
      return 0;
   *name_root = name_node(node);
   return 1;
}

/* C#: types live under namespace_declaration → declaration_list (a container, descended
 * below), so this fires on the type/method nodes themselves. */
static int classify_csharp(TSNode node, const char **kind, TSNode *name_root)
{
   static const char *const F[] = {"method_declaration", "constructor_declaration",
                                   "local_function_statement", NULL};
   static const char *const T[] = {"class_declaration",
                                   "struct_declaration",
                                   "interface_declaration",
                                   "enum_declaration",
                                   "record_declaration",
                                   "delegate_declaration",
                                   NULL};
   if (!kind_lookup(ts_node_type(node), F, T, kind))
      return 0;
   *name_root = name_node(node);
   return 1;
}

/* Java: top-level types (methods are nested in class bodies — a follow-up). */
static int classify_java(TSNode node, const char **kind, TSNode *name_root)
{
   static const char *const F[] = {"method_declaration", "constructor_declaration", NULL};
   static const char *const T[] = {
       "class_declaration",  "interface_declaration",       "enum_declaration",
       "record_declaration", "annotation_type_declaration", NULL};
   if (!kind_lookup(ts_node_type(node), F, T, kind))
      return 0;
   *name_root = name_node(node);
   return 1;
}

/* Ruby: methods and class/module definitions. */
static int classify_ruby(TSNode node, const char **kind, TSNode *name_root)
{
   static const char *const F[] = {"method", "singleton_method", NULL};
   static const char *const T[] = {"class", "module", "singleton_class", NULL};
   if (!kind_lookup(ts_node_type(node), F, T, kind))
      return 0;
   *name_root = name_node(node);
   return 1;
}

/* PHP: functions/methods and class-like types. */
static int classify_php(TSNode node, const char **kind, TSNode *name_root)
{
   static const char *const F[] = {"function_definition", "method_declaration", NULL};
   static const char *const T[] = {"class_declaration", "interface_declaration",
                                   "trait_declaration", "enum_declaration", NULL};
   if (!kind_lookup(ts_node_type(node), F, T, kind))
      return 0;
   *name_root = name_node(node);
   return 1;
}

/* Lua: function declarations (the name field carries dotted names like `T.m`). */
static int classify_lua(TSNode node, const char **kind, TSNode *name_root)
{
   if (strcmp(ts_node_type(node), "function_declaration") != 0)
      return 0;
   TSNode nm = ts_node_child_by_field_name(node, "name", 4);
   if (ts_node_is_null(nm))
      return 0;
   *kind = "function";
   *name_root = nm;
   return 1;
}

/* Bash: function definitions. */
static int classify_bash(TSNode node, const char **kind, TSNode *name_root)
{
   if (strcmp(ts_node_type(node), "function_definition") != 0)
      return 0;
   *kind = "function";
   *name_root = name_node(node);
   return 1;
}

/* Swift: functions and class/struct/enum (all class_declaration) + protocols. */
static int classify_swift(TSNode node, const char **kind, TSNode *name_root)
{
   static const char *const F[] = {"function_declaration", "init_declaration", NULL};
   static const char *const T[] = {"class_declaration", "protocol_declaration", NULL};
   if (!kind_lookup(ts_node_type(node), F, T, kind))
      return 0;
   *name_root = name_node(node);
   return 1;
}

/* Kotlin: definitions expose no `name` field, so name_node digs the simple_identifier /
 * type_identifier out of the node. */
static int classify_kotlin(TSNode node, const char **kind, TSNode *name_root)
{
   static const char *const F[] = {"function_declaration", NULL};
   static const char *const T[] = {"class_declaration", "object_declaration", NULL};
   if (!kind_lookup(ts_node_type(node), F, T, kind))
      return 0;
   *name_root = name_node(node);
   return 1;
}

/* Dart: function/method signatures and class-like types. */
static int classify_dart(TSNode node, const char **kind, TSNode *name_root)
{
   static const char *const F[] = {"function_signature", "method_signature", NULL};
   static const char *const T[] = {"class_definition", "enum_declaration", "mixin_declaration",
                                   "extension_declaration", NULL};
   if (!kind_lookup(ts_node_type(node), F, T, kind))
      return 0;
   *name_root = name_node(node);
   return 1;
}

/* CSS has no functions/types; the one named, referenceable construct is a @keyframes
 * animation. Surface its name as a "type"; selectors (rule_set) are not symbols. */
static int classify_css(TSNode node, const char **kind, TSNode *name_root)
{
   if (strcmp(ts_node_type(node), "keyframes_statement") != 0)
      return 0;
   TSNode nm;
   if (!child_of_type(node, "keyframes_name", &nm))
      return 0;
   *kind = "type";
   *name_root = nm;
   return 1;
}

static int classify(ts_lang_t lang, TSNode node, const char **kind, TSNode *name_root)
{
   switch (lang)
   {
   case TSL_C:
      return classify_c(node, kind, name_root);
   case TSL_CPP:
      return classify_cpp(node, kind, name_root);
   case TSL_CSHARP:
      return classify_csharp(node, kind, name_root);
   case TSL_PY:
      return classify_py(node, kind, name_root);
   case TSL_GO:
      return classify_go(node, kind, name_root);
   case TSL_JS:
   case TSL_TS:
      return classify_jsts(node, kind, name_root);
   case TSL_RUST:
      return classify_rust(node, kind, name_root);
   case TSL_JAVA:
      return classify_java(node, kind, name_root);
   case TSL_RUBY:
      return classify_ruby(node, kind, name_root);
   case TSL_PHP:
      return classify_php(node, kind, name_root);
   case TSL_LUA:
      return classify_lua(node, kind, name_root);
   case TSL_BASH:
      return classify_bash(node, kind, name_root);
   case TSL_SWIFT:
      return classify_swift(node, kind, name_root);
   case TSL_KOTLIN:
      return classify_kotlin(node, kind, name_root);
   case TSL_DART:
      return classify_dart(node, kind, name_root);
   case TSL_CSS:
      return classify_css(node, kind, name_root);
   }
   return 0;
}

/* Organizational wrappers we descend THROUGH to reach the definitions inside (namespaces,
 * their declaration lists, C++ templates/extern blocks). We do NOT descend function or
 * class bodies, so nested members (methods) are not surfaced — a follow-up. */
static int is_container(const char *t)
{
   return strcmp(t, "namespace_declaration") == 0 ||
          strcmp(t, "file_scoped_namespace_declaration") == 0 ||
          strcmp(t, "namespace_definition") == 0 || strcmp(t, "declaration_list") == 0 ||
          strcmp(t, "template_declaration") == 0 || strcmp(t, "linkage_specification") == 0;
}

/* Emit `node` if it is a definition, then descend into it if it is a container. */
static void visit(ts_lang_t lang, TSNode node, const char *content, definition_t *out, int max,
                  int *count)
{
   if (*count >= max)
      return;
   const char *kind = NULL;
   TSNode name_root;
   if (classify(lang, node, &kind, &name_root) && !ts_node_is_null(name_root))
   {
      node_text(name_root, content, out[*count].name, (int)sizeof(out[*count].name));
      if (out[*count].name[0])
      {
         snprintf(out[*count].kind, sizeof(out[*count].kind), "%s", kind);
         out[*count].line = (int)ts_node_start_point(node).row + 1;
         out[*count].line_end = (int)ts_node_end_point(node).row + 1;
         (*count)++;
         if (*count >= max)
            return;
      }
   }
   if (is_container(ts_node_type(node)))
   {
      uint32_t n = ts_node_named_child_count(node);
      for (uint32_t i = 0; i < n && *count < max; i++)
         visit(lang, ts_node_named_child(node, i), content, out, max, count);
   }
}

int code_treesitter_definitions(const char *ext, const char *content, definition_t *out, int max)
{
   ts_lang_t which;
   const TSLanguage *lang = ts_language_for_ext(ext, &which);
   if (!lang || !content || !out || max <= 0)
      return -1;

   TSParser *parser = ts_parser_new();
   if (!parser)
      return -1;
   if (!ts_parser_set_language(parser, lang))
   {
      ts_parser_delete(parser);
      return -1;
   }
   TSTree *tree = ts_parser_parse_string(parser, NULL, content, (uint32_t)strlen(content));
   if (!tree)
   {
      ts_parser_delete(parser);
      return -1;
   }

   TSNode root = ts_tree_root_node(tree);
   int count = 0;
   uint32_t top = ts_node_named_child_count(root);
   for (uint32_t i = 0; i < top && count < max; i++)
      visit(which, ts_node_named_child(root, i), content, out, max, &count);

   ts_tree_delete(tree);
   ts_parser_delete(parser);
   return count;
}

#else /* !AIMEE_TREESITTER */

int code_treesitter_available(const char *ext)
{
   (void)ext;
   return 0;
}

int code_treesitter_definitions(const char *ext, const char *content, definition_t *out, int max)
{
   (void)ext;
   (void)content;
   (void)out;
   (void)max;
   return -1;
}

#endif
