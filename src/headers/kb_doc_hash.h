/* kb_doc_hash.h: shared content hashes for staged KB documents. */
#pragma once

#define KB_DOC_HASH_HEX_LEN 64

void kb_doc_content_hash(const char *bytes, int nbytes, char out[KB_DOC_HASH_HEX_LEN + 1]);

/* Frontmatter-insensitive content hash for Markdown/MDX (graph-feedback §5): hashes
 * the document body plus its YAML frontmatter with an explicit allowlist of
 * IGNORABLE keys removed (status, reviewed, tags, date — and their multi-line
 * values), so a metadata-only edit doesn't re-bill extraction while a title or
 * body edit always does. A doc without a leading `---` frontmatter block hashes
 * identically to kb_doc_content_hash. Same 64-hex output. */
void kb_doc_content_hash_md(const char *bytes, int nbytes, char out[KB_DOC_HASH_HEX_LEN + 1]);

/* Dispatch by extension: a Markdown/MDX path uses the frontmatter-insensitive
 * hash; everything else uses the exact content hash. NULL/unknown path → exact. */
void kb_doc_content_hash_for_path(const char *path, const char *bytes, int nbytes,
                                  char out[KB_DOC_HASH_HEX_LEN + 1]);
