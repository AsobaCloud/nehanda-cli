#ifndef DEC_CLI_TUI_H
#define DEC_CLI_TUI_H 1

int builtin_chat_loop(const char *sock, const char *agent_name, const char *model,
                      const char *session_id, int autonomous, int debug, int default_launch,
                      int argc, char **argv);

/* One-shot chat turn (no stdout output): runs `message` through aimee-server and
 * returns the reply (heap, caller frees) or NULL on error. Used by acp-serve. */
char *cli_chat_once(const char *sock, const char *session_id, const char *message);

/* Like cli_chat_once but streams: text_cb(delta, ud) fires per incremental text
 * chunk. Returns the full reply (heap, caller frees) or NULL. No stdout output. */
char *cli_chat_stream(const char *sock, const char *session_id, const char *message,
                      void (*text_cb)(const char *delta, void *ud),
                      void (*tool_cb)(const char *phase, const char *tool_name, void *ud),
                      void *ud);


/* promoted from a former .inc (cross-TU) */
char *join_message_args(int argc, char **argv);
#endif /* DEC_CLI_TUI_H */
