/* cmd_init.c: stack detection and .aimee-rules generation for aimee init */
#include "commands.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define RULES_FILE ".aimee-rules"

static int has_file_in_dir(const char *dir, const char *filename)
{
   char path[MAX_PATH_LEN];
   snprintf(path, sizeof(path), "%s/%s", dir, filename);
   return access(path, F_OK) == 0;
}

static int dir_has_c_files(const char *dir)
{
   DIR *d = opendir(dir);
   if (!d)
      return 0;
   struct dirent *entry;
   while ((entry = readdir(d)) != NULL)
   {
      size_t n = strlen(entry->d_name);
      if (n > 2 && strcmp(entry->d_name + n - 2, ".c") == 0)
      {
         closedir(d);
         return 1;
      }
   }
   closedir(d);
   return 0;
}

int detect_project_stacks(const char *dir, stack_info_t out[], int max_stacks)
{
   int n = 0;

   /* CMake/C++: check before plain C to avoid double-counting */
   if (n < max_stacks && has_file_in_dir(dir, "CMakeLists.txt"))
   {
      out[n].name = "CMake/C++";
      out[n].build_cmd = "cmake --build .";
      out[n].test_cmd = "ctest";
      n++;
   }
   /* C: Makefile + at least one .c file in the root */
   if (n < max_stacks && has_file_in_dir(dir, "Makefile") && dir_has_c_files(dir))
   {
      out[n].name = "C";
      out[n].build_cmd = "make";
      out[n].test_cmd = "make test";
      n++;
   }
   /* Rust */
   if (n < max_stacks && has_file_in_dir(dir, "Cargo.toml"))
   {
      out[n].name = "Rust";
      out[n].build_cmd = "cargo build";
      out[n].test_cmd = "cargo test";
      n++;
   }
   /* Go */
   if (n < max_stacks && has_file_in_dir(dir, "go.mod"))
   {
      out[n].name = "Go";
      out[n].build_cmd = "go build ./...";
      out[n].test_cmd = "go test ./...";
      n++;
   }
   /* TypeScript: package.json + tsconfig.json */
   if (n < max_stacks && has_file_in_dir(dir, "package.json") &&
       has_file_in_dir(dir, "tsconfig.json"))
   {
      out[n].name = "TypeScript";
      out[n].build_cmd = "npm run build";
      out[n].test_cmd = "npm test";
      n++;
   }
   else if (n < max_stacks && has_file_in_dir(dir, "package.json"))
   {
      /* Node.js without TypeScript */
      out[n].name = "Node.js";
      out[n].build_cmd = "npm install";
      out[n].test_cmd = "npm test";
      n++;
   }
   /* Python */
   if (n < max_stacks &&
       (has_file_in_dir(dir, "pyproject.toml") || has_file_in_dir(dir, "setup.py")))
   {
      out[n].name = "Python";
      out[n].build_cmd = "pip install -e .";
      out[n].test_cmd = "pytest";
      n++;
   }

   return n;
}

int write_aimee_rules_file(const char *dir, const stack_info_t *stacks, int count)
{
   char path[MAX_PATH_LEN];
   snprintf(path, sizeof(path), "%s/%s", dir, RULES_FILE);

   /* Idempotent: skip if already exists */
   if (access(path, F_OK) == 0)
      return 1;

   FILE *f = fopen(path, "w");
   if (!f)
      return -1;

   const char *project_name = strrchr(dir, '/');
   project_name = project_name ? project_name + 1 : dir;

   fprintf(f, "# Project: %s\n\n", project_name);
   fprintf(f, "## Detected stack\n");

   if (count > 0)
   {
      for (int i = 0; i < count; i++)
         fprintf(f, "- %s\n", stacks[i].name);
   }
   else
   {
      fprintf(f, "- (none detected)\n");
   }

   fprintf(f, "\n## Verification\n");
   if (count > 0)
   {
      for (int i = 0; i < count; i++)
      {
         fprintf(f, "- Build: `%s`\n", stacks[i].build_cmd);
         fprintf(f, "- Test: `%s`\n", stacks[i].test_cmd);
      }
   }
   else
   {
      fprintf(f, "- Run: (configure in this file)\n");
   }

   fprintf(f, "\n## Working agreement\n"
              "- Prefer small, reviewable changes\n"
              "- Update tests alongside code changes\n");

   fclose(f);
   return 0;
}

int write_novel_rules_file(const char *dir)
{
   char path[MAX_PATH_LEN];
   snprintf(path, sizeof(path), "%s/%s", dir, RULES_FILE);

   /* Idempotent: skip if already exists */
   if (access(path, F_OK) == 0)
      return 1;

   FILE *f = fopen(path, "w");
   if (!f)
      return -1;

   const char *project_name = strrchr(dir, '/');
   project_name = project_name ? project_name + 1 : dir;

   fprintf(f, "# Story Bible: %s\n\n", project_name);
   fprintf(f, "Creative-writing project. The primary agent works as a fiction\n"
              "author and worldbuilder. Keep this bible authoritative: scenes must\n"
              "stay consistent with the canon recorded here and in memory.\n\n");

   fprintf(f, "## Premise\n"
              "- (one or two sentences: what is this story about?)\n\n");

   fprintf(f, "## Voice & POV\n"
              "- Narrative voice: (e.g. close third, wry, lyrical)\n"
              "- POV: (e.g. single POV / rotating; whose?)\n"
              "- Tense: (past / present)\n\n");

   fprintf(f, "## Main Characters\n"
              "- (name) — role, defining traits, what they want\n\n");

   fprintf(f, "## Setting\n"
              "- (place / world / era and the rules that govern it)\n\n");

   fprintf(f, "## Timeline & Canon\n"
              "- (key established events, in order; add as the story grows)\n\n");

   fprintf(f, "## Working agreement\n"
              "- Recall the relevant canon before writing a scene.\n"
              "- Never silently contradict established facts; reconcile first.\n"
              "- Preserve the author's prose; revise only what is asked.\n"
              "- Record new canon here (or in memory) so later scenes stay consistent.\n");

   fclose(f);
   return 0;
}

int write_songbook_rules_file(const char *dir)
{
   char path[MAX_PATH_LEN];
   snprintf(path, sizeof(path), "%s/%s", dir, RULES_FILE);

   /* Idempotent: skip if already exists */
   if (access(path, F_OK) == 0)
      return 1;

   FILE *f = fopen(path, "w");
   if (!f)
      return -1;

   const char *project_name = strrchr(dir, '/');
   project_name = project_name ? project_name + 1 : dir;

   fprintf(f, "# Songbook: %s\n\n", project_name);
   fprintf(f, "Songwriting project. The primary agent works as a songwriter and\n"
              "lyricist. Keep this songbook authoritative: sections must stay\n"
              "consistent with the voice, hook, and structure recorded here.\n\n");

   fprintf(f, "## Concept\n"
              "- (theme / what is this song about, in a line)\n"
              "- Mood: (e.g. wistful, defiant, tender)\n\n");

   fprintf(f, "## Voice\n"
              "- Narrator: (who is singing, to whom?)\n"
              "- Tense / point of view:\n\n");

   fprintf(f, "## Structure\n"
              "- Form: (e.g. verse / pre-chorus / chorus / verse / chorus / bridge / chorus)\n"
              "- Rhyme scheme per section:\n"
              "- Meter / line length:\n\n");

   fprintf(f, "## Hook\n"
              "- (the central, most memorable line — the chorus's anchor)\n\n");

   fprintf(f, "## Working agreement\n"
              "- Recall the concept, voice, and structure before writing a section.\n"
              "- Mind meter and rhyme; flag lines that do not scan rather than forcing them.\n"
              "- Preserve the author's lyrics; revise only what is asked.\n"
              "- Record hook and structural choices here so later sections stay consistent.\n");

   fclose(f);
   return 0;
}
