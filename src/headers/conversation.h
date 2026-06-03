/* conversation.h: conversation branching and thread exploration */
#ifndef DEC_CONVERSATION_H
#define DEC_CONVERSATION_H 1

#include "cJSON.h"
#include <stddef.h>

#define CONV_MAX_THREADS 16
#define CONV_MAX_LABEL   64

/* A single conversation thread — a fork of the message history from a branch
 * point.  Thread 0 is always the root (main) thread. */
typedef struct
{
   int id;
   int parent_id;        /* -1 for the root thread */
   int branch_msg_index; /* message index in the parent where this thread forked */
   char label[CONV_MAX_LABEL];
   cJSON *messages; /* this thread's full message history (shared prefix + own) */
} conv_thread_t;

/* Thread manager for a single chat session. */
typedef struct
{
   conv_thread_t threads[CONV_MAX_THREADS];
   int count;
   int active; /* index into threads[] */
} conv_threads_t;

/* Initialise thread state with a root thread holding |initial_messages|.
 * Takes ownership of the cJSON array (caller must not free it). */
void conv_threads_init(conv_threads_t *ct, cJSON *initial_messages);

/* Create a new branch from the active thread at its current message index.
 * |label| is optional (may be NULL). Returns the new thread id (>0) on
 * success, or -1 on failure (e.g. max threads reached). */
int conv_branch(conv_threads_t *ct, const char *label);

/* Switch the active thread to |thread_id|.
 * Returns 0 on success, -1 if thread_id is invalid. */
int conv_switch(conv_threads_t *ct, int thread_id);

/* Return a pointer to the active thread's messages array.  Never NULL after
 * conv_threads_init(). */
cJSON *conv_active_messages(const conv_threads_t *ct);

/* Return the active thread id. */
int conv_active_id(const conv_threads_t *ct);

/* Free all thread state (including message arrays). */
void conv_threads_free(conv_threads_t *ct);

#endif /* DEC_CONVERSATION_H */
