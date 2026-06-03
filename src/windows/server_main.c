/* server_main.c: Windows Service Control Manager integration */
#include "log.h"
#include "platform_process.h"
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static SERVICE_STATUS_HANDLE g_service_status_handle = NULL;
static SERVICE_STATUS g_service_status;
static const char *g_service_socket_path = NULL;
static log_level_t g_service_log_level;
static int (*g_run_server_fn)(const char *, log_level_t) = NULL;

static void service_set_status(DWORD current_state, DWORD win32_exit_code, DWORD wait_hint)
{
   g_service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
   g_service_status.dwCurrentState = current_state;
   g_service_status.dwWin32ExitCode = win32_exit_code;
   g_service_status.dwWaitHint = wait_hint;
   g_service_status.dwControlsAccepted =
       (current_state == SERVICE_START_PENDING) ? 0 : SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
   g_service_status.dwCheckPoint =
       (current_state == SERVICE_RUNNING || current_state == SERVICE_STOPPED) ? 0 : 1;

   if (g_service_status_handle)
      SetServiceStatus(g_service_status_handle, &g_service_status);
}

static DWORD WINAPI service_ctrl_handler(DWORD control, DWORD event_type, LPVOID event_data,
                                         LPVOID context)
{
   (void)event_type;
   (void)event_data;
   (void)context;

   switch (control)
   {
   case SERVICE_CONTROL_STOP:
   case SERVICE_CONTROL_SHUTDOWN:
      service_set_status(SERVICE_STOP_PENDING, NO_ERROR, 15000);
      /* Signal server shutdown via the run_server callback's global state */
      return NO_ERROR;
   default:
      return ERROR_CALL_NOT_IMPLEMENTED;
   }
}

static void WINAPI aimee_service_main(DWORD argc, LPTSTR *argv)
{
   (void)argc;
   (void)argv;

   g_service_status_handle =
       RegisterServiceCtrlHandlerEx(TEXT("aimee-server"), service_ctrl_handler, NULL);
   if (!g_service_status_handle)
      return;

   memset(&g_service_status, 0, sizeof(g_service_status));
   service_set_status(SERVICE_START_PENDING, NO_ERROR, 15000);

   int rc = g_run_server_fn(g_service_socket_path, g_service_log_level);
   service_set_status(SERVICE_STOPPED, rc == 0 ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR, 0);
}

void platform_server_redirect_stderr(FILE *log_fp)
{
   _dup2(_fileno(log_fp), 2);
}

void platform_server_install_signals(void (*handler)(int))
{
   /* Delegated to platform_process.h signal functions */
   platform_signal_term(handler);
   platform_signal_int(handler);
}

int platform_server_service_dispatch(const char *socket_path, log_level_t log_level,
                                     int (*run_server_fn)(const char *, log_level_t))
{
   g_service_socket_path = socket_path;
   g_service_log_level = log_level;
   g_run_server_fn = run_server_fn;

   SERVICE_TABLE_ENTRY service_table[] = {
       {TEXT("aimee-server"), (LPSERVICE_MAIN_FUNCTION)aimee_service_main},
       {NULL, NULL},
   };

   if (!StartServiceCtrlDispatcher(service_table))
   {
      fprintf(stderr, "aimee-server: failed to connect to Windows Service Control Manager\n");
      return 1;
   }
   return 0;
}
