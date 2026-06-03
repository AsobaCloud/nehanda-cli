/* kb_export_obsidian.h: Obsidian markdown renderer for aimee kb export. */
#ifndef DEC_KB_EXPORT_OBSIDIAN_H
#define DEC_KB_EXPORT_OBSIDIAN_H 1
#include "cJSON.h"
/* Render the kb.export response object as Obsidian-compatible markdown files in out_dir.
 * Creates out_dir if it does not exist. Writes one .md file per memory entry and
 * one .md file per exported entity profile.
 * Returns 0 on success, -1 if directory creation or file writing fails. */
int kb_export_obsidian_render(cJSON *export_obj, const char *out_dir);
#endif
