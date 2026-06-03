---
name: find-symbols
description: Use when locating where a symbol, function, type, command, or constant is defined or referenced, instead of grep/find/read chains.
source: bundled
triggers:
  tool: [Bash]
  arg_pattern: ["rg ", "grep ", "find "]
---

# Find Symbols

Use Aimee's code index before starting a broad text search for a code symbol.

Preferred route:

1. Use the `find_symbol` tool or run `aimee index find <symbol>` for definitions and references.
2. If the result is too broad, add enough namespace, file, or project context to narrow the symbol.
3. Read only the files that the index identifies as relevant.
4. Fall back to `rg` only when the index has no entry, is stale, or the target is plain text rather than a code symbol.

The index is meant to avoid exploratory grep/find/read chains. If it gives a precise file and line, use that as the starting point.
