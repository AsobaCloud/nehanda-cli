/* code_treesitter.c: §2 tree-sitter extraction front-end. See code_treesitter.h.
 *
 * Real implementation under -DAIMEE_TREESITTER (links the fetched tree-sitter runtime +
 * grammars); otherwise a stub that reports "unavailable" so callers fall back to the
 * hand-rolled extractors and the default build needs no tree-sitter at all. */

#include "headers/code_treesitter.h"

#include <string.h>

#ifdef AIMEE_TREESITTER

#include "tree_sitter/api.h"
#include <stdio.h>

/* Grammar entry points (defined in the vendored parser.c files). Add a language by
 * vendoring its grammar (scripts/fetch-treesitter.sh + src/Makefile), declaring its
 * entry point here, mapping its extension(s) in ts_language_for_ext, and adding a
 * classify_<lang> for that grammar's definition node types (see classify below). */
const TSLanguage *tree_sitter_c(void);
const TSLanguage *tree_sitter_python(void);
const TSLanguage *tree_sitter_go(void);
const TSLanguage *tree_sitter_javascript(void);
const TSLanguage *tree_sitter_rust(void);

typedef enum
{
   TSL_C,
   TSL_PY,
   TSL_GO,
   TSL_JS,
   TSL_RUST
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
       {".py", TSL_PY, tree_sitter_python},
       {".go", TSL_GO, tree_sitter_go},
       {".js", TSL_JS, tree_sitter_javascript},
       {".mjs", TSL_JS, tree_sitter_javascript},
       {".cjs", TSL_JS, tree_sitter_javascript},
       {".jsx", TSL_JS, tree_sitter_javascript},
       {".rs", TSL_RUST, tree_sitter_rust},
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
 * the definition's name (e.g. the function name under a function_declarator). */
static int first_identifier(TSNode node, TSNode *out)
{
   const char *t = ts_node_type(node);
   if (strcmp(t, "identifier") == 0 || strcmp(t, "type_identifier") == 0 ||
       strcmp(t, "field_identifier") == 0)
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

/* Each classify_<lang> maps a top-level node to a (kind, name-bearing subtree),
 * returning 0 if it is not a definition we surface. kind is "function" (callables) or
 * "type" (class/struct/enum/trait/typedef). name_root is the subtree whose first
 * identifier is the symbol name (see first_identifier) — it may be a possibly-null
 * field node; the caller re-checks ts_node_is_null. Only the file's TOP-LEVEL named
 * children are walked, so nested members (methods in a class/impl) are not surfaced;
 * recursive descent is a follow-up. */

/* C: the name survives pointer/array declarator wrapping, so name_root is the whole
 * declarator and first_identifier digs out the bare name. */
static int classify_c(TSNode node, const char **kind, TSNode *name_root)
{
   const char *t = ts_node_type(node);
   if (strcmp(t, "function_definition") == 0)
   {
      *kind = "function";
      *name_root = ts_node_child_by_field_name(node, "declarator", 10);
      return 1;
   }
   if (strcmp(t, "declaration") == 0)
   {
      /* a top-level function prototype: declarator is a function_declarator. */
      TSNode d = ts_node_child_by_field_name(node, "declarator", 10);
      if (!ts_node_is_null(d) && strstr(ts_node_type(d), "function_declarator"))
      {
         *kind = "function";
         *name_root = d;
         return 1;
      }
      return 0;
   }
   if (strcmp(t, "type_definition") == 0)
   {
      *kind = "type"; /* typedef: the new name is the declarator */
      *name_root = ts_node_child_by_field_name(node, "declarator", 10);
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
   *name_root = ts_node_child_by_field_name(node, "name", 4);
   return 1;
}

/* Go: func/method declarations and named types (type_spec inside a type_declaration,
 * including the first spec of a grouped `type ( ... )` block). */
static int classify_go(TSNode node, const char **kind, TSNode *name_root)
{
   const char *t = ts_node_type(node);
   if (strcmp(t, "function_declaration") == 0 || strcmp(t, "method_declaration") == 0)
   {
      *kind = "function";
      *name_root = ts_node_child_by_field_name(node, "name", 4);
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

/* JavaScript: function (incl. generator) and class declarations, including those behind
 * an `export`/`export default`. Arrow functions bound to a const/let are not surfaced
 * (no name field) — a follow-up. */
static int classify_js(TSNode node, const char **kind, TSNode *name_root)
{
   const char *t = ts_node_type(node);
   if (strcmp(t, "export_statement") == 0)
   {
      TSNode inner = ts_node_child_by_field_name(node, "declaration", 11);
      if (ts_node_is_null(inner))
         return 0; /* re-export / `export const x = ...` — no named declaration */
      return classify_js(inner, kind, name_root);
   }
   if (strcmp(t, "function_declaration") == 0 || strcmp(t, "generator_function_declaration") == 0)
      *kind = "function";
   else if (strcmp(t, "class_declaration") == 0)
      *kind = "type";
   else
      return 0;
   *name_root = ts_node_child_by_field_name(node, "name", 4);
   return 1;
}

/* Rust: functions and the named type-like items. */
static int classify_rust(TSNode node, const char **kind, TSNode *name_root)
{
   const char *t = ts_node_type(node);
   if (strcmp(t, "function_item") == 0)
      *kind = "function";
   else if (strcmp(t, "struct_item") == 0 || strcmp(t, "enum_item") == 0 ||
            strcmp(t, "trait_item") == 0 || strcmp(t, "type_item") == 0 ||
            strcmp(t, "union_item") == 0)
      *kind = "type";
   else
      return 0;
   *name_root = ts_node_child_by_field_name(node, "name", 4);
   return 1;
}

static int classify(ts_lang_t lang, TSNode node, const char **kind, TSNode *name_root)
{
   switch (lang)
   {
   case TSL_C:
      return classify_c(node, kind, name_root);
   case TSL_PY:
      return classify_py(node, kind, name_root);
   case TSL_GO:
      return classify_go(node, kind, name_root);
   case TSL_JS:
      return classify_js(node, kind, name_root);
   case TSL_RUST:
      return classify_rust(node, kind, name_root);
   }
   return 0;
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
   {
      TSNode child = ts_node_named_child(root, i);
      const char *kind = NULL;
      TSNode name_root;
      if (!classify(which, child, &kind, &name_root) || ts_node_is_null(name_root))
         continue;
      TSNode id;
      if (!first_identifier(name_root, &id))
         continue;
      node_text(id, content, out[count].name, (int)sizeof(out[count].name));
      if (!out[count].name[0])
         continue;
      snprintf(out[count].kind, sizeof(out[count].kind), "%s", kind);
      out[count].line = (int)ts_node_start_point(child).row + 1;
      out[count].line_end = (int)ts_node_end_point(child).row + 1;
      count++;
   }

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
