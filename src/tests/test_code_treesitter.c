/* test_code_treesitter.c: §2 tree-sitter front-end. Built + run ONLY in the opt-in
 * AIMEE_TREESITTER build (it links the fetched runtime + grammar). Parses real C and
 * asserts the extracted definition_t set matches the hand-rolled extractor's shape. */
#include "headers/code_treesitter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int has(const definition_t *d, int n, const char *name, const char *kind)
{
   for (int i = 0; i < n; i++)
      if (strcmp(d[i].name, name) == 0 && strcmp(d[i].kind, kind) == 0)
         return 1;
   return 0;
}

int main(void)
{
   printf("test_code_treesitter:\n");

   /* grammar availability gates the dispatch. */
   assert(code_treesitter_available(".c"));
   assert(code_treesitter_available(".h"));
   assert(!code_treesitter_available(".py")); /* no python grammar vendored */
   assert(!code_treesitter_available(NULL));

   const char *src = "#include <stdio.h>\n"
                     "typedef struct Point { int x; int y; } Point;\n"
                     "static int add(int a, int b) { return a + b; }\n"
                     "void greet(const char *who);\n" /* prototype */
                     "int main(void) { return add(1, 2); }\n";
   definition_t defs[32];
   int n = code_treesitter_definitions(".c", src, defs, 32);
   assert(n >= 4);

   assert(has(defs, n, "add", "function"));    /* function_definition */
   assert(has(defs, n, "main", "function"));   /* function_definition */
   assert(has(defs, n, "greet", "function"));  /* prototype declaration */
   assert(has(defs, n, "Point", "type"));      /* typedef */

   /* line numbers are 1-based and plausible. */
   for (int i = 0; i < n; i++)
   {
      assert(defs[i].line >= 1);
      assert(defs[i].line_end >= defs[i].line);
   }

   /* an ext with no grammar -> -1 (caller falls back to the hand-rolled extractor). */
   assert(code_treesitter_definitions(".py", src, defs, 32) == -1);

   /* bounded output: max=1 yields exactly one. */
   assert(code_treesitter_definitions(".c", src, defs, 1) == 1);

   printf("  extracted %d defs (add/main/greet/Point)\n", n);
   printf("ALL PASS\n");
   return 0;
}
