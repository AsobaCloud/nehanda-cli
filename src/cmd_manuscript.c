/* cmd_manuscript.c: `aimee manuscript` — scenes / wordcount / outline / check.
 *
 * Runs client-side (like `clean` and `delegate plan`) so it sees the user's
 * working tree directly. Pure manuscript helpers live in manuscript.c; this
 * file is the command surface and the continuity done-gate. */
#include "manuscript.h"
#include "aimee_client.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef MANUSCRIPT_PATH_MAX
#define MANUSCRIPT_PATH_MAX 4096
#endif

static void ms_emit_json(cJSON *root)
{
   char *s = cJSON_PrintUnformatted(root);
   if (s)
   {
      puts(s);
      free(s);
   }
   cJSON_Delete(root);
}

static int ms_scenes(int with_words, int json_output)
{
   char cwd[MANUSCRIPT_PATH_MAX];
   if (!getcwd(cwd, sizeof(cwd)))
      snprintf(cwd, sizeof(cwd), ".");

   manuscript_entry_t files[MANUSCRIPT_MAX_FILES];
   long total = 0;
   int n = manuscript_scan(cwd, files, MANUSCRIPT_MAX_FILES, &total);

   if (json_output)
   {
      cJSON *root = cJSON_CreateObject();
      cJSON *arr = cJSON_AddArrayToObject(root, "files");
      for (int i = 0; i < n; i++)
      {
         cJSON *item = cJSON_CreateObject();
         cJSON_AddStringToObject(item, "name", files[i].name);
         cJSON_AddNumberToObject(item, "words", (double)files[i].words);
         cJSON_AddItemToArray(arr, item);
      }
      cJSON_AddNumberToObject(root, "files_count", n);
      cJSON_AddNumberToObject(root, "total_words", (double)total);
      ms_emit_json(root);
      return 0;
   }

   if (n == 0)
   {
      fprintf(stderr, "No manuscript files (.md/.txt) found here.\n");
      return 0;
   }
   for (int i = 0; i < n; i++)
   {
      if (with_words)
         printf("%8ld  %s\n", files[i].words, files[i].name);
      else
         printf("%s\n", files[i].name);
   }
   if (with_words)
      printf("%8ld  TOTAL (%d file%s)\n", total, n, n == 1 ? "" : "s");
   return 0;
}

static int ms_outline(int json_output)
{
   static const char *candidates[] = {"OUTLINE.md", "outline.md", "Outline.md",
                                      "BEATS.md",   "beats.md",   NULL};
   for (int i = 0; candidates[i]; i++)
   {
      if (access(candidates[i], F_OK) != 0)
         continue;
      char *text = manuscript_read_file(candidates[i]);
      if (!text)
         continue;
      if (json_output)
      {
         cJSON *root = cJSON_CreateObject();
         cJSON_AddStringToObject(root, "outline_file", candidates[i]);
         cJSON_AddStringToObject(root, "outline", text);
         ms_emit_json(root);
      }
      else
      {
         fputs(text, stdout);
         if (text[0] && text[strlen(text) - 1] != '\n')
            fputc('\n', stdout);
      }
      free(text);
      return 0;
   }
   fprintf(stderr,
           "No outline found (looked for OUTLINE.md / BEATS.md). Create one to track structure.\n");
   return 0;
}

/* Resolve the done-gate delegate role + verdict marker + the active persona
 * name from the active persona over the aimee-server /v1 API (GET /v1/persona) —
 * the thin client never reads aimee config files. Defaults to
 * continuity/CONTINUITY/engineer when the server is unreachable or the persona
 * declares no gate. */
static void ms_gate_from_api(char *role, size_t rn, char *marker, size_t mn, char *persona,
                             size_t pn)
{
   snprintf(role, rn, "continuity");
   snprintf(marker, mn, "CONTINUITY");
   snprintf(persona, pn, "engineer");
   int st = 0;
   char *resp = aimee_client_request("GET", "/v1/persona", NULL, &st);
   if (resp && st == 200)
   {
      cJSON *root = cJSON_Parse(resp);
      cJSON *cr = root ? cJSON_GetObjectItemCaseSensitive(root, "check_role") : NULL;
      cJSON *cm = root ? cJSON_GetObjectItemCaseSensitive(root, "check_marker") : NULL;
      cJSON *nm = root ? cJSON_GetObjectItemCaseSensitive(root, "name") : NULL;
      if (cJSON_IsString(cr) && cr->valuestring[0])
         snprintf(role, rn, "%s", cr->valuestring);
      if (cJSON_IsString(cm) && cm->valuestring[0])
         snprintf(marker, mn, "%s", cm->valuestring);
      if (cJSON_IsString(nm) && nm->valuestring[0])
         snprintf(persona, pn, "%s", nm->valuestring);
      cJSON_Delete(root);
   }
   free(resp);
}

/* Done-gate: run the active persona's review delegate (continuity for novel,
 * prosody for songwriter, ...) over the target files and fail (return 1) on an
 * unresolved issue. Targets default to all files when none are named. */
static int ms_check(int argc, char **argv)
{
   char role[64], marker[64], persona_name[64];
   ms_gate_from_api(role, sizeof(role), marker, sizeof(marker), persona_name, sizeof(persona_name));
   char fail_marker[80], instruction[256];
   snprintf(fail_marker, sizeof(fail_marker), "%s: FAIL", marker);
   snprintf(instruction, sizeof(instruction),
            "Review these files against the persona's standard, then end with a line "
            "'%s: PASS' or '%s: FAIL'",
            marker, marker);
   const char *verb = role;
   char targets[MANUSCRIPT_PATH_MAX] = "";
   size_t tpos = 0;
   if (argc > 1)
   {
      for (int i = 1; i < argc && tpos < sizeof(targets) - 1; i++)
         tpos += (size_t)snprintf(targets + tpos, sizeof(targets) - tpos, "%s%s", tpos ? " " : "",
                                  argv[i]);
   }
   else
   {
      char cwd[MANUSCRIPT_PATH_MAX];
      if (!getcwd(cwd, sizeof(cwd)))
         snprintf(cwd, sizeof(cwd), ".");
      manuscript_entry_t files[MANUSCRIPT_MAX_FILES];
      int n = manuscript_scan(cwd, files, MANUSCRIPT_MAX_FILES, NULL);
      for (int i = 0; i < n && tpos < sizeof(targets) - 1; i++)
         tpos += (size_t)snprintf(targets + tpos, sizeof(targets) - tpos, "%s%s", tpos ? " " : "",
                                  files[i].name);
   }

   if (!targets[0])
   {
      fprintf(stderr, "manuscript check: nothing to check.\n");
      return 1;
   }

   /* The check delegate runs as the active persona (novel/songwriter/...), which
    * is required for every delegate. */
   char cmd[MANUSCRIPT_PATH_MAX + 512];
   snprintf(cmd, sizeof(cmd), "aimee delegate %s --persona %s \"%s: %s\"", role, persona_name,
            instruction, targets);

   FILE *p = popen(cmd, "r");
   if (!p)
   {
      fprintf(stderr, "manuscript check: failed to run %s delegate.\n", verb);
      return 1;
   }
   int failed = 0;
   char line[4096];
   while (fgets(line, sizeof(line), p))
   {
      fputs(line, stdout);
      if (strstr(line, fail_marker))
         failed = 1;
   }
   int rc = pclose(p);
   if (failed)
   {
      fprintf(stderr, "\nmanuscript check: FAILED — unresolved %s issue.\n", verb);
      return 1;
   }
   if (rc != 0)
   {
      fprintf(stderr, "\nmanuscript check: %s delegate did not complete cleanly.\n", verb);
      return 1;
   }
   fprintf(stderr, "\nmanuscript check: %s clean.\n", verb);
   return 0;
}

/* argv[0] is the subcommand (scenes|wordcount|outline|check), as passed by the
 * cli_main dispatcher (the leading "manuscript" has already been stripped). */
int cmd_manuscript_run(int argc, char **argv, int json_output)
{
   const char *sub = argc > 0 ? argv[0] : "scenes";

   if (strcmp(sub, "scenes") == 0)
      return ms_scenes(0, json_output);
   if (strcmp(sub, "wordcount") == 0)
      return ms_scenes(1, json_output);
   if (strcmp(sub, "outline") == 0)
      return ms_outline(json_output);
   if (strcmp(sub, "check") == 0)
      return ms_check(argc, argv);

   fprintf(stderr, "usage: aimee manuscript <scenes|wordcount|outline|check [files...]>\n");
   return 2;
}
