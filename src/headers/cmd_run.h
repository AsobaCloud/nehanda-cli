/* cmd_run.h: public types for cmd_run.c (exposed for unit tests) */
#ifndef DEC_CMD_RUN_H
#define DEC_CMD_RUN_H 1

/* Exit codes for aimee run */
#define RUN_EXIT_OK        0
#define RUN_EXIT_ERROR     1
#define RUN_EXIT_LIMIT_HIT 2

typedef enum
{
   RUN_OUTPUT_TEXT = 0,
   RUN_OUTPUT_JSON,
   RUN_OUTPUT_NDJSON
} run_output_mode_t;

/* Parse --output flag value to run_output_mode_t.
 * Returns RUN_OUTPUT_TEXT for NULL or unknown values. */
run_output_mode_t run_parse_output_mode(const char *s);

#endif /* DEC_CMD_RUN_H */
