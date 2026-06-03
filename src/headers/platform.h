/*
 * platform.h: platform detection and common portability macros.
 *
 * Defines AIMEE_LINUX, AIMEE_MACOS, AIMEE_WINDOWS so downstream code
 * can select the right implementation at compile time.
 */
#ifndef DEC_PLATFORM_H
#define DEC_PLATFORM_H 1

/* --- Platform detection --- */

#if defined(_WIN32) || defined(_WIN64)
#define AIMEE_WINDOWS 1
#elif defined(__APPLE__) && defined(__MACH__)
#define AIMEE_MACOS 1
#elif defined(__linux__)
#define AIMEE_LINUX 1
#else
#error "Unsupported platform: expected Linux, macOS, or Windows"
#endif

/* AIMEE_POSIX is set on any Unix-like system (Linux, macOS) */
#if defined(AIMEE_LINUX) || defined(AIMEE_MACOS)
#define AIMEE_POSIX 1
#endif

/* --- Executable extension --- */

#ifdef AIMEE_WINDOWS
#define AIMEE_EXE_EXT ".exe"
#else
#define AIMEE_EXE_EXT ""
#endif

/* --- Portable handle type --- */

#ifdef AIMEE_WINDOWS
/* winsock2.h must be included before windows.h to avoid redefinition errors */
#include <winsock2.h>
#include <windows.h>
typedef HANDLE platform_fd_t;
#define PLATFORM_INVALID_FD INVALID_HANDLE_VALUE
#else
typedef int platform_fd_t;
#define PLATFORM_INVALID_FD (-1)
#endif

/* --- Portable time --- */

#include <time.h>
#ifdef AIMEE_WINDOWS
/* timegm is POSIX-only; MinGW provides _mkgmtime */
#define timegm _mkgmtime
#endif

/*
 * --- POSIX compatibility shims for Windows (MinGW) ---
 *
 * MinGW already provides most POSIX functions (getcwd, access, chmod, unlink,
 * getpid, fileno, fdopen, popen, pclose, usleep, gmtime_r, localtime_r,
 * strtok_r, strcasecmp, strncasecmp).  Only add shims for things it lacks.
 */

#ifdef AIMEE_WINDOWS
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* POSIX types not available on Windows */
#ifndef _POSIX
typedef unsigned int uid_t;
typedef unsigned int gid_t;
#ifndef _PID_T_
typedef int pid_t;
#define _PID_T_
#endif
#endif

/* realpath: MinGW lacks this; use _fullpath */
static inline char *realpath(const char *path, char *resolved)
{
   if (resolved)
      return _fullpath(resolved, path, _MAX_PATH);
   char *buf = (char *)malloc(_MAX_PATH);
   if (!buf)
      return NULL;
   if (!_fullpath(buf, path, _MAX_PATH))
   {
      free(buf);
      return NULL;
   }
   return buf;
}

/* lstat: MinGW has no distinct lstat; map to stat */
#define lstat stat

/* Stat mode macros missing on Windows */
#define S_ISLNK(m) 0
#ifndef S_ISSOCK
#define S_ISSOCK(m) 0
#endif

/* strcasestr: not in C standard or MinGW */
static inline char *strcasestr(const char *haystack, const char *needle)
{
   if (!needle[0])
      return (char *)haystack;
   size_t nlen = strlen(needle);
   for (; *haystack; haystack++)
   {
      if (_strnicmp(haystack, needle, nlen) == 0)
         return (char *)haystack;
   }
   return NULL;
}

/* gmtime_r / localtime_r: MinGW may lack these without _POSIX_C_SOURCE.
 * Use the non-reentrant versions (safe enough for single-threaded callers). */
#ifndef gmtime_r
static inline struct tm *aimee_gmtime_r(const time_t *t, struct tm *result)
{
   struct tm *tmp = gmtime(t);
   if (tmp)
   {
      *result = *tmp;
      return result;
   }
   return NULL;
}
#define gmtime_r aimee_gmtime_r
#endif
#ifndef localtime_r
static inline struct tm *aimee_localtime_r(const time_t *t, struct tm *result)
{
   struct tm *tmp = localtime(t);
   if (tmp)
   {
      *result = *tmp;
      return result;
   }
   return NULL;
}
#define localtime_r aimee_localtime_r
#endif

/* setsid: no-op on Windows (used for daemonization) */
#define setsid() ((int)0)

/* strptime: MinGW lacks this; minimal implementation for "%Y-%m-%dT%H:%M:%S" */
#include <stdio.h>
static inline char *strptime(const char *s, const char *fmt, struct tm *tm)
{
   (void)fmt; /* we only handle the one format used in this codebase */
   int n = sscanf(s, "%d-%d-%dT%d:%d:%d", &tm->tm_year, &tm->tm_mon, &tm->tm_mday, &tm->tm_hour,
                  &tm->tm_min, &tm->tm_sec);
   if (n < 6)
      return NULL;
   tm->tm_year -= 1900;
   tm->tm_mon -= 1;
   return (char *)s + 19; /* length of "YYYY-MM-DDTHH:MM:SS" */
}

#endif /* AIMEE_WINDOWS */

/* --- Platform default notification command --- */
#ifdef AIMEE_MACOS
#define PLATFORM_DEFAULT_NOTIFY_CMD                                                                \
   "osascript -e 'display notification \"{{message}}\" with title \"aimee\"'"
#elif defined(AIMEE_WINDOWS)
#define PLATFORM_DEFAULT_NOTIFY_CMD ""
#else
#define PLATFORM_DEFAULT_NOTIFY_CMD "notify-send 'aimee' '{{message}}'"
#endif

/* Portable pclose() exit code extraction */
#ifdef AIMEE_WINDOWS
static inline int platform_pclose_status(int status)
{
   return status;
}
#else
#include <sys/wait.h>
static inline int platform_pclose_status(int status)
{
   return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
#endif

#endif /* DEC_PLATFORM_H */
