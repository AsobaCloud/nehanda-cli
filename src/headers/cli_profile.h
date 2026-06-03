/* cli_profile.h: -p / --profile flag extraction for the
 * aimee-profiles-and-usage-insights proposal.
 *
 * The flag is consumed before any state import so AIMEE_PROFILE
 * propagates into aimee_home() (src/aimee_home.h) and onward to
 * every path-resolution call site. Pure helper; cli_main.c is
 * responsible for setenv() + argv shifting.
 *
 * Recognised forms:
 *   -p NAME
 *   --profile NAME
 *   --profile=NAME */
#ifndef DEC_CLI_PROFILE_H
#define DEC_CLI_PROFILE_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

   /* Scan argv for the profile flag. On match, copies the profile
    * name into `out` (NUL-terminated, truncated to outsz-1) and
    * shifts argv left to remove the consumed slots; *argc is
    * decremented accordingly. Returns 1 on match, 0 on no flag.
    *
    * Returns 0 (no consume) when the flag appears without a value
    * ("-p" at end of argv) or when the value is empty — those land
    * as a usage error in the caller, not a silent profile switch.
    *
    * Pure: no env mutation, no syscalls, no allocation. argv slots
    * are not freed (caller owns them). */
   int cli_extract_profile(int *argc, char **argv, char *out, size_t outsz);

#ifdef __cplusplus
}
#endif

#endif /* DEC_CLI_PROFILE_H */
