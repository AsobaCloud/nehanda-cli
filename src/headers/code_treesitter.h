/* code_treesitter.h: §2 tree-sitter extraction front-end.
 *
 * An optional, broader-coverage replacement for the hand-rolled extractors
 * (extractors.c), feeding the SAME definition_t symbol table. Built only when
 * compiled with -DAIMEE_TREESITTER against the fetched tree-sitter runtime + grammars
 * (scripts/fetch-treesitter.sh); otherwise every entry point reports "unavailable" and
 * the caller falls back to the hand-rolled path, so the default build is unchanged. */
#ifndef DEC_CODE_TREESITTER_H
#define DEC_CODE_TREESITTER_H 1

#include "index.h" /* definition_t, call_ref_t */

/* 1 if the tree-sitter front-end is compiled in AND has a grammar for `ext`
 * (e.g. ".c"/".h"); 0 otherwise (caller uses extract_definitions instead). */
int code_treesitter_available(const char *ext);

/* Extract top-level definitions (functions, typedefs, struct/union/enum types) from
 * `content` using the tree-sitter grammar for `ext`. Fills up to `max` definition_t
 * (name/kind/line/line_end). Returns the count, or -1 if tree-sitter is unavailable or
 * has no grammar for `ext` (the caller then falls back to extract_definitions). */
int code_treesitter_definitions(const char *ext, const char *content, definition_t *out, int max);

/* Extract call edges (caller -> callee, by name) from `content` using the tree-sitter
 * grammar for `ext`. The caller is the enclosing function/method (empty at file scope);
 * the callee is the called function/method name (the last identifier of `obj.m()`). Fills
 * up to `max` call_ref_t. Returns the count, or -1 if tree-sitter is unavailable, has no
 * grammar for `ext`, or does not extract calls for that language (the caller then falls
 * back to the hand-rolled extract_calls). */
int code_treesitter_calls(const char *ext, const char *content, call_ref_t *out, int max);

#endif /* DEC_CODE_TREESITTER_H */
