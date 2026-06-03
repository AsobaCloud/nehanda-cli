/* cmd_infra.c: Windows stub for background index scan (no fork on Windows) */

void platform_infra_background_scan(const char *cwd)
{
   (void)cwd;
   /* Background scan via fork is POSIX-only; no-op on Windows. */
}
