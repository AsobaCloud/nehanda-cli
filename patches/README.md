# Patches

Diffs applied on top of the aimee upstream (`upstream/`) to integrate Nehanda-specific behaviour.

Patches are numbered sequentially and applied in order at build time. They are kept separate from `src/` (new files) to make upstream tracking easier — when pulling a new upstream version, patches are re-applied and conflicts are resolved here.

## Convention

```
NNN-short-description.patch
```

- `NNN` — zero-padded sequence number (001, 002, ...)
- Short description — what the patch does, kebab-case

## Applying patches manually

```bash
cd upstream
for p in ../patches/*.patch; do
  git apply "$p"
done
```

## Creating a new patch

Make your change inside `upstream/`, then:

```bash
cd upstream
git diff > ../patches/NNN-your-description.patch
git checkout .   # revert — the patch is the record, not the upstream file
```

## Current patches

| File | Description |
|---|---|
| `001-macos-sock-compat.patch` | macOS `SOCK_CLOEXEC` / `accept4()` compat |
| `002-nehanda-monolithic.patch` | In-process SQLite hooks for unified nehanda binary |
