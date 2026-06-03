/* cmd_slop.c: aimee slop — scan files for AI-slop patterns and report findings.
 *
 * Usage:
 *   aimee slop <file> [<file> ...]   — scan specific files
 *   aimee slop --changed             — scan files changed vs HEAD
 *   aimee slop --staged              — scan staged files
 *   aimee slop --fix <file> [...]    — delegate cleanup and recheck
 *
 * Findings are advisory only.  Exit code:
 *   0 — no findings
 *   1 — findings reported
 *   2 — scan error (file not found, etc.)
 */
#include "aimee.h"
#include "commands.h"
#include "log.h"
#include "slop_detect.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SLOP_FINDINGS 64

/* Print findings for a single file to stderr (or stdout in JSON mode). */
static int report_file(app_ctx_t *ctx, const char *path, int *total_findings)
{
   slop_finding_t findings[MAX_SLOP_FINDINGS];
   int n = slop_detect_file(path, findings, MAX_SLOP_FINDINGS);
   if (n < 0)
   {
      LOG_ERROR("slop", "cannot read %s", path);
      return -1;
   }
   if (n == 0)
      return 0;

   *total_findings += n;

   if (ctx && ctx->json_output)
   {
      /* JSON output: one object per finding */
      for (int i = 0; i < n; i++)
      {
         printf("{\"file\":\"%s\",\"line\":%d,\"category\":\"%s\",\"excerpt\":\"%s\"}\n", path,
                findings[i].line_number, slop_category_label(findings[i].category),
                findings[i].excerpt);
      }
   }
   else
   {
      for (int i = 0; i < n; i++)
      {
         fprintf(stderr, "%s:%d: [%s] %s\n", path, findings[i].line_number,
                 slop_category_label(findings[i].category), findings[i].excerpt);
      }
   }
   return n;
}

/* Collect files from `git diff --name-only [--cached] HEAD` */
static int scan_git_changed(app_ctx_t *ctx, int staged, int *total_findings)
{
   const char *git_cmd = staged ? "git diff --name-only --cached HEAD 2>/dev/null"
                                : "git diff --name-only HEAD 2>/dev/null";

   FILE *fp = popen(git_cmd, "r");
   if (!fp)
   {
      LOG_ERROR("slop", "cannot run git diff");
      return -1;
   }

   int scanned = 0;
   char line[MAX_PATH_LEN];
   while (fgets(line, sizeof(line), fp))
   {
      /* Strip trailing newline */
      line[strcspn(line, "\r\n")] = '\0';
      if (!line[0])
         continue;
      int rc = report_file(ctx, line, total_findings);
      if (rc >= 0)
         scanned++;
   }
   pclose(fp);
   return scanned;
}

/* Delegate a cleanup for a single file using the findings as context.
 * Returns 0 on success, -1 on failure. */
static int fix_file(const char *path, slop_finding_t *findings, int nfound)
{
   /* Build a prompt describing the findings */
   char prompt[4096];
   int pos = snprintf(prompt, sizeof(prompt),
                      "Clean up AI-slop patterns in %s. "
                      "Remove only the following flagged items — do not change anything else:\n",
                      path);
   for (int i = 0; i < nfound && pos < (int)sizeof(prompt) - 120; i++)
      pos += snprintf(prompt + pos, sizeof(prompt) - (size_t)pos, "  line %d [%s]: %s\n",
                      findings[i].line_number, slop_category_label(findings[i].category),
                      findings[i].excerpt);
   /* The last loop append can return a would-be length that runs pos past the
    * buffer (a long excerpt); clamp before the final append so its
    * sizeof(prompt) - pos does not wrap to a huge size_t and write OOB. */
   if (pos > (int)sizeof(prompt))
      pos = (int)sizeof(prompt);
   pos += snprintf(prompt + pos, sizeof(prompt) - (size_t)pos,
                   "Keep all code, business-logic comments, and boundary-validation. "
                   "Only remove the listed AI-slop lines.");

   /* Run: aimee delegate general "<prompt>" */
   char cmd[5120];
   snprintf(cmd, sizeof(cmd), "aimee delegate general %s 2>&1", prompt);
   /* Shell-quote the prompt to handle spaces and special chars */
   char qprompt[4500];
   int qi = 0;
   qprompt[qi++] = '\'';
   for (int i = 0; prompt[i] && qi < (int)sizeof(qprompt) - 3; i++)
   {
      if (prompt[i] == '\'')
      {
         qprompt[qi++] = '\'';
         qprompt[qi++] = '\\';
         qprompt[qi++] = '\'';
         qprompt[qi++] = '\'';
      }
      else
         qprompt[qi++] = prompt[i];
   }
   qprompt[qi++] = '\'';
   qprompt[qi] = '\0';

   snprintf(cmd, sizeof(cmd), "aimee delegate general --persona engineer %s 2>&1", qprompt);
   fprintf(stderr, "aimee slop --fix: delegating cleanup for %s ...\n", path);
   FILE *fp = popen(cmd, "r");
   if (!fp)
   {
      LOG_ERROR("slop", "cannot run delegate for %s", path);
      return -1;
   }
   char line[512];
   while (fgets(line, sizeof(line), fp))
      fputs(line, stderr);
   int exit_code = pclose(fp);
   if (exit_code != 0)
   {
      LOG_ERROR("slop", "delegate exited with code %d for %s", exit_code, path);
      return -1;
   }
   return 0;
}

void cmd_slop(app_ctx_t *ctx, int argc, char **argv)
{
   if (argc == 0)
   {
      fprintf(stderr, "Usage: aimee slop <file> [<file> ...]\n"
                      "       aimee slop --changed\n"
                      "       aimee slop --staged\n"
                      "       aimee slop --fix <file> [<file> ...]\n"
                      "\n"
                      "Scans files for AI-slop patterns and reports findings.\n"
                      "With --fix, delegates a targeted cleanup then rechecks.\n"
                      "Patterns detected:\n"
                      "  attribution   — AI attribution markers (\"generated by\", etc.)\n"
                      "  hollow-todo   — TODO/FIXME with no description\n"
                      "  lint-suppression — suppression directives without a reason\n"
                      "  type-annotation  — comment is only a type name\n"
                      "  placeholder      — explicit stub/placeholder markers\n"
                      "\n"
                      "Exit code: 0 = clean, 1 = findings, 2 = error\n");
      return;
   }

   int total_findings = 0;
   int errors = 0;
   int fix_mode = 0;

   /* Check for --fix flag */
   for (int i = 0; i < argc; i++)
   {
      if (strcmp(argv[i], "--fix") == 0)
      {
         fix_mode = 1;
         break;
      }
   }

   for (int i = 0; i < argc; i++)
   {
      if (strcmp(argv[i], "--fix") == 0)
         continue;

      if (strcmp(argv[i], "--changed") == 0)
      {
         int rc = scan_git_changed(ctx, 0, &total_findings);
         if (rc < 0)
            errors++;
      }
      else if (strcmp(argv[i], "--staged") == 0)
      {
         int rc = scan_git_changed(ctx, 1, &total_findings);
         if (rc < 0)
            errors++;
      }
      else if (fix_mode)
      {
         /* In fix mode: detect, delegate cleanup, then recheck */
         slop_finding_t findings[MAX_SLOP_FINDINGS];
         int n = slop_detect_file(argv[i], findings, MAX_SLOP_FINDINGS);
         if (n < 0)
         {
            LOG_ERROR("slop", "cannot read %s", argv[i]);
            errors++;
            continue;
         }
         if (n == 0)
         {
            fprintf(stderr, "%s: no slop findings — nothing to fix.\n", argv[i]);
            continue;
         }

         fprintf(stderr, "%s: %d finding(s) before fix:\n", argv[i], n);
         for (int fi = 0; fi < n; fi++)
            fprintf(stderr, "  line %d [%s] %s\n", findings[fi].line_number,
                    slop_category_label(findings[fi].category), findings[fi].excerpt);

         if (fix_file(argv[i], findings, n) != 0)
         {
            errors++;
            continue;
         }

         /* Recheck after delegation */
         slop_finding_t after[MAX_SLOP_FINDINGS];
         int nafter = slop_detect_file(argv[i], after, MAX_SLOP_FINDINGS);
         if (nafter < 0)
         {
            LOG_ERROR("slop", "cannot recheck %s", argv[i]);
            errors++;
         }
         else if (nafter == 0)
            fprintf(stderr, "%s: clean after fix.\n", argv[i]);
         else
         {
            fprintf(stderr, "%s: %d finding(s) remain after fix:\n", argv[i], nafter);
            for (int fi = 0; fi < nafter; fi++)
               fprintf(stderr, "  line %d [%s] %s\n", after[fi].line_number,
                       slop_category_label(after[fi].category), after[fi].excerpt);
            total_findings += nafter;
         }
      }
      else
      {
         int rc = report_file(ctx, argv[i], &total_findings);
         if (rc < 0)
            errors++;
      }
   }

   if (!ctx || !ctx->json_output)
   {
      if (total_findings > 0)
         fprintf(stderr, "\n%d finding(s) — advisory only, no changes made.\n", total_findings);
      else if (errors == 0 && !fix_mode)
         fprintf(stderr, "No slop findings.\n");
   }

   if (errors > 0)
      exit(2);
   if (total_findings > 0)
      exit(1);
}
