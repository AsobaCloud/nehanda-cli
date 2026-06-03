#ifndef DEC_KB_REFLECTION_H
#define DEC_KB_REFLECTION_H 1

#include <pthread.h>

typedef struct
{
   pthread_t thread;
   int active;
   volatile int stop;
} kb_reflection_ctx_t;

/* Initialise and start the reflection scheduler thread.
 * No-op if learning.review.scheduler_enabled is false (default). */
void kb_reflection_init(kb_reflection_ctx_t *ctx);

/* Stop the reflection scheduler thread and wait for it to exit. */
void kb_reflection_shutdown(kb_reflection_ctx_t *ctx);

#endif /* DEC_KB_REFLECTION_H */
