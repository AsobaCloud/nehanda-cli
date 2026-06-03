/* kb_ingest_normalize.h: document format detection and normalization for kb ingest.
 *
 * Detects format by filename extension and normalizes bytes to markdown.
 * See docs/proposals/pending/aimee-kb-ingest-api-and-corpus-staging.md */
#pragma once

/* Detect format converter from filename extension.
 * Returns one of: "passthrough", "pandoc", "pdftotext", "code", "ocr_stub".
 * filename may be NULL (returns "passthrough"). */
const char *kb_ingest_detect_format(const char *filename);

/* Normalize file bytes to markdown text.
 * filename: original filename (used for extension detection and code fence tag).
 * bytes: raw file content (not necessarily null-terminated).
 * nbytes: byte count.
 * out_buf: output buffer for null-terminated markdown text.
 * out_cap: capacity of out_buf.
 * converter_out: receives converter name (e.g. "passthrough"), capacity conv_cap.
 * converter_version_out: receives converter version string, capacity ver_cap.
 * Returns 0 on success, -1 on subprocess failure. */
int kb_ingest_normalize(const char *filename, const char *bytes, int nbytes, char *out_buf,
                        int out_cap, char *converter_out, int conv_cap, char *converter_version_out,
                        int ver_cap);
