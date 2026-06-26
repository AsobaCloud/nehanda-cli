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
   assert(code_treesitter_available(".py"));
   assert(code_treesitter_available(".go"));
   assert(code_treesitter_available(".js"));
   assert(code_treesitter_available(".jsx"));
   assert(code_treesitter_available(".rs"));
   assert(!code_treesitter_available(".ts")); /* typescript grammar not vendored yet */
   assert(!code_treesitter_available(".txt"));
   assert(!code_treesitter_available(NULL));

   definition_t defs[32];

   /* --- C --- */
   const char *c_src = "#include <stdio.h>\n"
                       "typedef struct Point { int x; int y; } Point;\n"
                       "static int add(int a, int b) { return a + b; }\n"
                       "void greet(const char *who);\n" /* prototype */
                       "int main(void) { return add(1, 2); }\n";
   int n = code_treesitter_definitions(".c", c_src, defs, 32);
   assert(n >= 4);
   assert(has(defs, n, "add", "function"));   /* function_definition */
   assert(has(defs, n, "main", "function"));  /* function_definition */
   assert(has(defs, n, "greet", "function")); /* prototype declaration */
   assert(has(defs, n, "Point", "type"));     /* typedef */
   for (int i = 0; i < n; i++)
   {
      assert(defs[i].line >= 1);
      assert(defs[i].line_end >= defs[i].line);
   }
   /* bounded output: max=1 yields exactly one. */
   assert(code_treesitter_definitions(".c", c_src, defs, 1) == 1);

   /* --- Python (incl. a decorated def) --- */
   const char *py_src = "import os\n"
                        "def add(a, b):\n    return a + b\n"
                        "class Point:\n    def m(self):\n        pass\n"
                        "@deco\ndef wrapped():\n    pass\n";
   n = code_treesitter_definitions(".py", py_src, defs, 32);
   assert(has(defs, n, "add", "function"));     /* function_definition */
   assert(has(defs, n, "Point", "type"));       /* class_definition */
   assert(has(defs, n, "wrapped", "function")); /* decorated_definition unwrapped */

   /* --- Go (func, method, named type) --- */
   const char *go_src = "package main\n"
                        "func Add(a, b int) int { return a + b }\n"
                        "func (p *Point) M() {}\n"
                        "type Point struct { X int }\n";
   n = code_treesitter_definitions(".go", go_src, defs, 32);
   assert(has(defs, n, "Add", "function")); /* function_declaration */
   assert(has(defs, n, "M", "function"));   /* method_declaration */
   assert(has(defs, n, "Point", "type"));   /* type_declaration -> type_spec */

   /* --- JavaScript (function, generator, class) --- */
   const char *js_src = "import x from 'y'\n"
                        "function add(a, b) { return a + b }\n"
                        "class Point { m() {} }\n"
                        "function* gen() {}\n"
                        "export function pub() {}\n" /* export_statement unwrap */
                        "export class Wid {}\n";     /* export_statement unwrap */
   n = code_treesitter_definitions(".js", js_src, defs, 32);
   assert(has(defs, n, "add", "function")); /* function_declaration */
   assert(has(defs, n, "Point", "type"));   /* class_declaration */
   assert(has(defs, n, "gen", "function")); /* generator_function_declaration */
   assert(has(defs, n, "pub", "function")); /* export function */
   assert(has(defs, n, "Wid", "type"));     /* export class */

   /* --- Rust (fn, struct, enum, trait) --- */
   const char *rs_src = "use std::io;\n"
                        "fn add(a: i32, b: i32) -> i32 { a + b }\n"
                        "struct Point { x: i32 }\n"
                        "enum E { A, B }\n"
                        "trait T {}\n";
   n = code_treesitter_definitions(".rs", rs_src, defs, 32);
   assert(has(defs, n, "add", "function")); /* function_item */
   assert(has(defs, n, "Point", "type"));   /* struct_item */
   assert(has(defs, n, "E", "type"));       /* enum_item */
   assert(has(defs, n, "T", "type"));       /* trait_item */

   /* an ext with no grammar -> -1 (caller falls back to the hand-rolled extractor). */
   assert(code_treesitter_definitions(".ts", c_src, defs, 32) == -1);

   printf("  C/Python/Go/JavaScript/Rust definitions extracted\n");
   printf("ALL PASS\n");
   return 0;
}
