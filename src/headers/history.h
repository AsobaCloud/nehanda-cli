/* history.h: persistent chat input history with terminal line editor */
#ifndef HISTORY_H
#define HISTORY_H 1

#define HISTORY_DEFAULT_MAX 200

typedef struct chat_history chat_history_t;

/* Open (or create) a history store at the given JSONL file path.
 * max_entries: maximum number of entries to keep (0 = HISTORY_DEFAULT_MAX). */
chat_history_t *history_open(const char *path, int max_entries);

/* Flush and free all resources. */
void history_close(chat_history_t *h);

/* Add an entry.  Ignores entries starting with '/'.
 * Suppresses duplicate consecutive entries. */
void history_add(chat_history_t *h, const char *entry);

/* Navigate backwards (older entries).  Returns NULL when at oldest. */
const char *history_prev(chat_history_t *h);

/* Navigate forwards (newer entries).  Returns NULL when past newest
 * (i.e., back at the current unsaved input). */
const char *history_next(chat_history_t *h);

/* Reset navigation index to "current input" position. */
void history_reset_nav(chat_history_t *h);

/* Number of entries currently stored. */
int history_count(const chat_history_t *h);

/* Returns non-zero if the navigation cursor is at the "current input" tip. */
int history_at_tip(const chat_history_t *h);

/* Register a list of words for tab completion.
 * When Tab is pressed and the line starts with '/', the typed prefix is
 * matched against words (case-sensitive prefix match) and candidates are
 * cycled on successive Tab presses.  Caller owns the array and strings;
 * they must remain valid until history_close() or the next call to
 * history_set_completions(). Pass words=NULL / count=0 to clear. */
void history_set_completions(chat_history_t *h, const char **words, int count);

/* Enable (1) or disable (0) vim editing mode.  When enabled, Esc
 * transitions from insert mode to normal mode (hjkl navigation, dd/x
 * deletion, 0/$ line boundaries, w/b word motion, i/a/A back to insert). */
void history_set_vim_mode(chat_history_t *h, int enabled);

/* Returns non-zero if vim mode is currently enabled. */
int history_vim_mode(const chat_history_t *h);

/* Read a line from stdin with history navigation (up/down arrows) and
 * basic line editing.  Prints prompt to stdout.  Returns a pointer to a
 * static buffer (overwritten on next call), or NULL on EOF.
 * Falls back to fgets when stdin is not a tty. */
char *history_readline(chat_history_t *h, const char *prompt);

#endif /* HISTORY_H */
