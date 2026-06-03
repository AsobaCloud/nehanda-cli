#ifndef AIMEE_SHUTDOWN_FORENSICS_H
#define AIMEE_SHUTDOWN_FORENSICS_H 1

#include <stddef.h>
#include <sys/types.h>
#include <time.h>

typedef struct
{
   char signal_name[16];
   int signal_number;
   pid_t sender_pid;
   char sender_comm[64];
   char daemon[24];
   pid_t daemon_pid;
   long uptime_s;
   int inflight_turns;
   int inflight_jobs;
   int inflight_workers;
   long rss_kb;
   time_t at;
   int unclean_exit;
   char process_tree[512];
} shutdown_ctx_t;

const char *shutdown_forensics_dir(void);
const char *shutdown_forensics_signal_name(int sig);
void shutdown_forensics_snapshot(shutdown_ctx_t *out, const char *daemon, int sig,
                                 const void *siginfo, time_t started_at, int inflight_turns,
                                 int inflight_jobs, int inflight_workers);
int shutdown_forensics_write_record(const shutdown_ctx_t *ctx);
int shutdown_forensics_spawn_async_diagnostic(const shutdown_ctx_t *ctx);
int shutdown_forensics_record_signal(const char *daemon, int sig, const void *siginfo,
                                     time_t started_at, int inflight_turns, int inflight_jobs,
                                     int inflight_workers);
int shutdown_forensics_mark_started(const char *daemon, time_t started_at);
int shutdown_forensics_mark_stopped(const char *daemon, pid_t pid);
int shutdown_forensics_record_unclean_exits(void);
int shutdown_forensics_list_recent(shutdown_ctx_t *rows, size_t cap);
int shutdown_forensics_rotate(int keep);
void shutdown_forensics_format_summary(const shutdown_ctx_t *ctx, char *buf, size_t buflen);

#endif /* AIMEE_SHUTDOWN_FORENSICS_H */
