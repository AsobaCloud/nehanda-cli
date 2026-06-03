/* server_coord_dispatcher.h: background thread that claims pending coord tasks
 * and dispatches them as delegate workers.
 *
 * coord jobs created by delegate.launch sit in the DB with status 'pending'
 * until this dispatcher claims and runs each task.  It respects the job's
 * max_concurrent limit and re-fires after each task completes or a new job
 * is created. */
#ifndef DEC_SERVER_COORD_DISPATCHER_H
#define DEC_SERVER_COORD_DISPATCHER_H 1

#include "server.h"

#ifdef __cplusplus
extern "C"
{
#endif

   /* Start the dispatcher background thread.  Idempotent; safe to call from
    * server_init unconditionally. */
   void server_coord_dispatcher_init(server_ctx_t *ctx);

   /* Stop the dispatcher thread.  Idempotent; safe to call from
    * server_shutdown unconditionally. */
   void server_coord_dispatcher_shutdown(void);

   /* Wake the dispatcher immediately (non-blocking).  Call after creating a
    * new coord job or after a coord task completes. */
   void server_coord_dispatcher_notify(void);

#ifdef __cplusplus
}
#endif

#endif /* DEC_SERVER_COORD_DISPATCHER_H */
