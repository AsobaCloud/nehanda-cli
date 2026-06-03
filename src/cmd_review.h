#ifndef DEC_CMD_REVIEW_H
#define DEC_CMD_REVIEW_H 1

#include "aimee.h"

/* Shared review loop for a single surface.
 * surface: "memory", "rule", "epistemic_directive", "anti_pattern", etc.
 * Supports: --limit N, --confidence-min F, --json */
void cmd_review_surface(const char *surface, app_ctx_t *ctx, int argc, char **argv);

/* Cross-surface review shorthand: fetches proposed artifacts from all
 * surfaces ordered by confidence DESC.  Registered as the top-level
 * "aimee review" command. */
void cmd_review_all(app_ctx_t *ctx, int argc, char **argv);

#endif /* DEC_CMD_REVIEW_H */
