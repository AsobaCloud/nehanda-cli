/* util.c: Windows stubs for POSIX-only utilities */
#include "aimee.h"
#include <stdlib.h>

int safe_exec_capture(const char *const argv[], char **out_buf, size_t max_out)
{
   (void)argv;
   (void)max_out;
   *out_buf = NULL;
   return -1;
}

int safe_exec_capture_env(const char *const argv[], char *const envp[], char **out_buf,
                          size_t max_out)
{
   (void)argv;
   (void)envp;
   (void)max_out;
   *out_buf = NULL;
   return -1;
}

int regex_match(const char *pattern, const char *text, int flags)
{
   (void)pattern;
   (void)text;
   (void)flags;
   return 0;
}
