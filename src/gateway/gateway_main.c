/* gateway_main.c: entry point for the ambient-presence gateway. */
#include "gateway_platform.h"
#include "gateway_ctx.h"
#include "shutdown_forensics.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;
static time_t g_started_at = 0;

#ifndef _WIN32
static void on_signal_info(int sig, siginfo_t *info, void *ucontext)
{
   (void)ucontext;
   (void)shutdown_forensics_record_signal("gateway", sig, info, g_started_at, 0, 0, 0);
   g_stop = 1;
}

static void install_signal_handlers(void)
{
   struct sigaction sa;
   memset(&sa, 0, sizeof(sa));
   sa.sa_sigaction = on_signal_info;
   sigemptyset(&sa.sa_mask);
   sa.sa_flags = SA_SIGINFO;
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);
#ifdef SIGHUP
   sigaction(SIGHUP, &sa, NULL);
#endif
}
#else
static void on_signal(int sig)
{
   (void)sig;
   g_stop = 1;
}

static void install_signal_handlers(void)
{
   signal(SIGINT, on_signal);
   signal(SIGTERM, on_signal);
}
#endif

static void usage(FILE *out)
{
   fprintf(out, "Usage: aimee-gateway [--check-config|--list-platforms]\n");
}

static int list_platforms(void)
{
   int count = 0;
   const gateway_platform_entry_t *entries = gateway_platform_entries(&count);
   for (int i = 0; i < count; i++)
      printf("%-10s %s\n", entries[i].name, entries[i].enabled ? "enabled" : "disabled");
   return 0;
}

static int check_config(void)
{
   int count = 0;
   int enabled = 0;
   const gateway_platform_entry_t *entries = gateway_platform_entries(&count);
   for (int i = 0; i < count; i++)
      enabled += entries[i].enabled ? 1 : 0;
   if (enabled == 0)
      fprintf(stderr, "aimee-gateway: no platform adapters configured; idling\n");
   return 0;
}

int main(int argc, char **argv)
{
   if (argc > 1)
   {
      if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)
      {
         usage(stdout);
         return 0;
      }
      if (strcmp(argv[1], "--list-platforms") == 0)
         return list_platforms();
      if (strcmp(argv[1], "--check-config") == 0)
         return check_config();
      usage(stderr);
      return 2;
   }

   if (check_config() != 0)
      return 1;

   g_started_at = time(NULL);
   install_signal_handlers();
   (void)shutdown_forensics_record_unclean_exits();
   (void)shutdown_forensics_mark_started("gateway", g_started_at);
   while (!g_stop)
      sleep(1);
   (void)shutdown_forensics_mark_stopped("gateway", getpid());
   return 0;
}
