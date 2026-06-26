#!/bin/sh
# fetch-treesitter.sh — fetch the tree-sitter runtime + grammars for the §2 opt-in
# tree-sitter extraction front-end (code_treesitter.c, built with AIMEE_TREESITTER=1).
#
# These are multi-MB GENERATED parsers, so they are fetched here rather than committed
# (see .gitignore). Pinned to specific commits for reproducibility. Idempotent: skips a
# grammar that is already present. Add a language by appending a `fetch <repo> <sha>`
# line and registering it in code_treesitter.c.
set -e

VENDOR="$(cd "$(dirname "$0")/../src/vendor" && pwd)"

fetch() {
    name="$1"; url="$2"; sha="$3"; dest="$VENDOR/$name"
    if [ -e "$dest/.fetched" ]; then
        echo "fetch-treesitter: $name present, skipping"
        return 0
    fi
    echo "fetch-treesitter: $name @ $sha"
    rm -rf "$dest"
    git clone -q "$url" "$dest"
    git -C "$dest" checkout -q "$sha"
    rm -rf "$dest/.git"
    touch "$dest/.fetched"
}

# Runtime (one amalgamated compilation unit via lib/src/lib.c).
fetch tree-sitter   https://github.com/tree-sitter/tree-sitter   cbee4672665173d1702d836353ef7648dc2b2fac
# Grammars (one parser.c each). C first — aimee dogfoods its own source.
fetch tree-sitter-c https://github.com/tree-sitter/tree-sitter-c b780e47fc780ddc8da13afa35a3f4ed5c157823d

echo "fetch-treesitter: done -> $VENDOR/tree-sitter{,-c}"
