/* test_code_treesitter.c: §2 tree-sitter front-end. Built + run ONLY in the opt-in
 * AIMEE_TREESITTER build (it links the fetched runtime + grammars). Parses real source in
 * every supported language and asserts the extracted definition_t set. */
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

/* Parse `src` as `ext` and assert (name, kind) is among the extracted definitions. */
static void want(const char *ext, const char *src, const char *name, const char *kind)
{
   definition_t d[64];
   int n = code_treesitter_definitions(ext, src, d, 64);
   if (n < 0 || !has(d, n, name, kind))
   {
      printf("  FAIL %s: want %s/%s, got %d defs:", ext, name, kind, n);
      for (int i = 0; i < n; i++)
         printf(" %s/%s", d[i].name, d[i].kind);
      printf("\n");
      assert(0);
   }
}

int main(void)
{
   printf("test_code_treesitter:\n");

   /* availability gates the dispatch (a representative extension per language). */
   const char *avail[] = {".c",   ".h",   ".cpp", ".cc",   ".hpp",   ".cs", ".py",   ".go",
                          ".js",  ".mjs", ".jsx", ".ts",   ".tsx",   ".rs", ".java", ".rb",
                          ".php", ".lua", ".sh",  ".bash", ".swift", ".kt", ".dart", ".css"};
   for (size_t i = 0; i < sizeof(avail) / sizeof(avail[0]); i++)
      assert(code_treesitter_available(avail[i]));
   assert(!code_treesitter_available(".txt"));
   assert(!code_treesitter_available(".md"));
   assert(!code_treesitter_available(NULL));

   /* --- C --- */
   const char *c_src = "typedef struct Point { int x; } Point;\n"
                       "static int add(int a, int b) { return a + b; }\n"
                       "void greet(const char *who);\n"
                       "int main(void) { return add(1, 2); }\n";
   definition_t d[64];
   int n = code_treesitter_definitions(".c", c_src, d, 64);
   assert(n >= 4);
   want(".c", c_src, "add", "function");
   want(".c", c_src, "greet", "function"); /* prototype */
   want(".c", c_src, "Point", "type");
   for (int i = 0; i < n; i++)
   {
      assert(d[i].line >= 1);
      assert(d[i].line_end >= d[i].line);
   }
   assert(code_treesitter_definitions(".c", c_src, d, 1) == 1); /* bounded */

   /* --- C++ (incl. a namespaced class — exercises container descent) --- */
   const char *cpp = "int add(int a){return a;}\nclass C{ void m(); };\nstruct S{};\n"
                     "namespace N { class Inner{}; }\n";
   want(".cpp", cpp, "add", "function");
   want(".cpp", cpp, "C", "type");
   want(".cpp", cpp, "S", "type");
   want(".cpp", cpp, "Inner", "type"); /* reached through namespace */

   /* --- C# (types live inside a namespace) --- */
   const char *cs =
       "namespace N { class C { void M(){} } interface I{} enum E{A} record R(int x); }\n"
       "class Top {}\n";
   want(".cs", cs, "C", "type");
   want(".cs", cs, "I", "type");
   want(".cs", cs, "R", "type");
   want(".cs", cs, "Top", "type"); /* top-level, no namespace */

   /* --- Python (incl. a decorated def) --- */
   const char *py = "def add(a, b):\n    return a + b\n"
                    "class Point:\n    pass\n"
                    "@deco\ndef wrapped():\n    pass\n";
   want(".py", py, "add", "function");
   want(".py", py, "Point", "type");
   want(".py", py, "wrapped", "function");

   /* --- Go --- */
   const char *go = "package main\nfunc Add(a int) int { return a }\n"
                    "func (p *Point) M() {}\ntype Point struct { X int }\n";
   want(".go", go, "Add", "function");
   want(".go", go, "M", "function");
   want(".go", go, "Point", "type");

   /* --- JavaScript (incl. export) --- */
   const char *js = "function add(a){return a}\nclass Point{}\nfunction* gen(){}\n"
                    "export function pub(){}\nexport class Wid{}\n";
   want(".js", js, "add", "function");
   want(".js", js, "Point", "type");
   want(".js", js, "gen", "function");
   want(".js", js, "pub", "function");
   want(".js", js, "Wid", "type");

   /* --- TypeScript --- */
   const char *ts = "function f(){}\nclass C{}\ninterface I{}\ntype T=number\nenum E{A}\n"
                    "export function h(){}\n";
   want(".ts", ts, "f", "function");
   want(".ts", ts, "C", "type");
   want(".ts", ts, "I", "type");
   want(".ts", ts, "T", "type");
   want(".ts", ts, "h", "function");
   want(".tsx", "export function App(){ return null }\n", "App", "function");

   /* --- Rust --- */
   const char *rs =
       "fn add(a: i32) -> i32 { a }\nstruct Point { x: i32 }\nenum E { A }\ntrait T {}\n";
   want(".rs", rs, "add", "function");
   want(".rs", rs, "Point", "type");
   want(".rs", rs, "E", "type");
   want(".rs", rs, "T", "type");

   /* --- Java (top-level types) --- */
   const char *java = "class C { void m(){} }\ninterface I{}\nenum E{A}\nrecord R(int x){}\n";
   want(".java", java, "C", "type");
   want(".java", java, "I", "type");
   want(".java", java, "R", "type");

   /* --- Ruby --- */
   const char *rb = "def foo; end\nclass C\nend\nmodule M\nend\n";
   want(".rb", rb, "foo", "function");
   want(".rb", rb, "C", "type");
   want(".rb", rb, "M", "type");

   /* --- PHP --- */
   const char *php = "<?php\nfunction f(){}\nclass C{}\ninterface I{}\ntrait Tr{}\n";
   want(".php", php, "f", "function");
   want(".php", php, "C", "type");
   want(".php", php, "Tr", "type");

   /* --- Lua (dotted name preserved) --- */
   const char *lua = "local function f() end\nfunction g() end\nfunction T.m() end\n";
   want(".lua", lua, "g", "function");
   want(".lua", lua, "T.m", "function");

   /* --- Bash --- */
   const char *sh = "foo() { echo hi; }\nfunction bar { echo yo; }\n";
   want(".sh", sh, "foo", "function");
   want(".sh", sh, "bar", "function");

   /* --- Swift --- */
   const char *swift = "func f(){}\nclass C{}\nstruct S{}\nprotocol P{}\n";
   want(".swift", swift, "f", "function");
   want(".swift", swift, "C", "type");
   want(".swift", swift, "P", "type");

   /* --- Kotlin (no name field — exercises identifier digging) --- */
   const char *kt = "fun f(){}\nclass C{}\nobject O{}\n";
   want(".kt", kt, "f", "function");
   want(".kt", kt, "C", "type");
   want(".kt", kt, "O", "type");

   /* --- Dart --- */
   const char *dart = "void f(){}\nclass C{}\nenum E{a}\n";
   want(".dart", dart, "f", "function");
   want(".dart", dart, "C", "type");

   /* --- CSS (@keyframes name) --- */
   want(".css", "@keyframes spin { from {} to {} }\n.cls { color: red }\n", "spin", "type");

   /* a vendored-but-unmapped ext -> -1 (caller falls back to the hand-rolled extractor). */
   assert(code_treesitter_definitions(".md", c_src, d, 64) == -1);

   printf("  all supported languages extracted (C/C++/C#/Python/Go/JS/TS/Rust/Java/"
          "Ruby/PHP/Lua/Bash/Swift/Kotlin/Dart/CSS)\n");
   printf("ALL PASS\n");
   return 0;
}
