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
 * vendoring its grammar + declaring + registering it in ts_language_for_ext below. */
const TSLanguage *tree_sitter_c(void);

static const TSLanguage *ts_language_for_ext(const char *ext)
{
   if (!ext)
      return NULL;
   if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0)
      return tree_sitter_c();
   return NULL;
}

int code_treesitter_available(const char *ext)
{
   return ts_language_for_ext(ext) != NULL;
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

/* Map a top-level node to a (kind, name-bearing subtree). Returns 0 if not a
 * definition we surface. The name is taken from the relevant field's first identifier
 * so it survives pointer/array declarator wrapping. */
static int classify(TSNode node, const char **kind, TSNode *name_root)
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

int code_treesitter_definitions(const char *ext, const char *content, definition_t *out, int max)
{
   const TSLanguage *lang = ts_language_for_ext(ext);
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
      if (!classify(child, &kind, &name_root) || ts_node_is_null(name_root))
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
