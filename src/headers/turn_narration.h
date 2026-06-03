/* turn_narration.h: rule-based turn summary for CLI chat and webchat */
#ifndef TURN_NARRATION_H
#define TURN_NARRATION_H 1

#include <stddef.h>

#define TURN_NAR_MAX_CALLS 64
#define TURN_NAR_NAME_LEN  64
#define TURN_NAR_ARG_LEN   128

/*
 * Accumulates tool call activity across all iterations of a single user turn.
 * Call turn_narration_add() after each tool execution batch, then
 * turn_narration_build() after the turn completes to get a summary string.
 *
 * Example output:
 *   "Modified src/auth.c; ran make test"
 *   "Read 3 files, wrote src/render.c; ran 2 commands"
 */
typedef struct
{
   char names[TURN_NAR_MAX_CALLS][TURN_NAR_NAME_LEN];
   char args[TURN_NAR_MAX_CALLS][TURN_NAR_ARG_LEN]; /* abbreviated key arg */
   int count;
   int iterations; /* number of tool-call iterations (LLM turns with tools) */
} turn_narration_t;

/* Reset the accumulator for a new user turn. */
void turn_narration_reset(turn_narration_t *nar);

/* Record one tool call. args_json may be NULL or a JSON object string. */
void turn_narration_add(turn_narration_t *nar, const char *name, const char *args_json);

/*
 * Increment the iteration counter (call once per tool-execution batch).
 * Used to distinguish "3 turns of tool use" from "1 turn of 3 tools".
 */
void turn_narration_inc_iter(turn_narration_t *nar);

/*
 * Write a human-readable summary into buf (NUL-terminated).
 * Returns 0 if there is nothing to summarise (count == 0),
 * or 1 if a summary was written.
 */
int turn_narration_build(const turn_narration_t *nar, char *buf, size_t len);

#endif /* TURN_NARRATION_H */
