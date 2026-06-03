#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diff.h"
#include "cJSON.h"

int main(void)
{
   printf("diff: ");

   /* --- identical content --- */
   {
      diff_result_t r;
      int rc = diff_compute("hello\nworld\n", "hello\nworld\n", &r);
      assert(rc == 0);
      assert(r.hunk_count == 0);
      assert(r.additions == 0);
      assert(r.deletions == 0);
      assert(r.truncated == 0);
   }

   /* --- new file (old is empty) --- */
   {
      diff_result_t r;
      int rc = diff_compute("", "line1\nline2\n", &r);
      assert(rc == 0);
      assert(r.additions == 2);
      assert(r.deletions == 0);
      assert(r.hunk_count == 1);
      assert(r.hunks[0].new_start == 1);
      assert(r.hunks[0].new_count == 2);
      assert(r.hunks[0].additions == 2);
   }

   /* --- delete all content --- */
   {
      diff_result_t r;
      int rc = diff_compute("line1\nline2\n", "", &r);
      assert(rc == 0);
      assert(r.additions == 0);
      assert(r.deletions == 2);
      assert(r.hunk_count == 1);
      assert(r.hunks[0].old_start == 1);
      assert(r.hunks[0].old_count == 2);
      assert(r.hunks[0].deletions == 2);
   }

   /* --- NULL inputs treated as empty --- */
   {
      diff_result_t r;
      int rc = diff_compute(NULL, "new\n", &r);
      assert(rc == 0);
      assert(r.additions == 1);

      rc = diff_compute("old\n", NULL, &r);
      assert(rc == 0);
      assert(r.deletions == 1);

      rc = diff_compute(NULL, NULL, &r);
      assert(rc == 0);
      assert(r.hunk_count == 0);
   }

   /* --- single line change --- */
   {
      diff_result_t r;
      int rc = diff_compute("aaa\nbbb\nccc\n", "aaa\nBBB\nccc\n", &r);
      assert(rc == 0);
      assert(r.additions == 1);
      assert(r.deletions == 1);
      assert(r.hunk_count == 1);
   }

   /* --- addition in middle --- */
   {
      diff_result_t r;
      int rc = diff_compute("aaa\nccc\n", "aaa\nbbb\nccc\n", &r);
      assert(rc == 0);
      assert(r.additions == 1);
      assert(r.deletions == 0);
      assert(r.hunk_count == 1);
   }

   /* --- deletion in middle --- */
   {
      diff_result_t r;
      int rc = diff_compute("aaa\nbbb\nccc\n", "aaa\nccc\n", &r);
      assert(rc == 0);
      assert(r.additions == 0);
      assert(r.deletions == 1);
      assert(r.hunk_count == 1);
   }

   /* --- multiple hunks (changes far apart) --- */
   {
      /* Create content with changes separated by many context lines */
      char old_buf[4096], new_buf[4096];
      int opos = 0, npos = 0;

      opos += snprintf(old_buf + opos, sizeof(old_buf) - opos, "changed-old\n");
      npos += snprintf(new_buf + npos, sizeof(new_buf) - npos, "changed-new\n");

      for (int i = 0; i < 20; i++)
      {
         opos += snprintf(old_buf + opos, sizeof(old_buf) - opos, "context-%d\n", i);
         npos += snprintf(new_buf + npos, sizeof(new_buf) - npos, "context-%d\n", i);
      }

      opos += snprintf(old_buf + opos, sizeof(old_buf) - opos, "also-old\n");
      npos += snprintf(new_buf + npos, sizeof(new_buf) - npos, "also-new\n");

      diff_result_t r;
      int rc = diff_compute(old_buf, new_buf, &r);
      assert(rc == 0);
      assert(r.additions == 2);
      assert(r.deletions == 2);
      assert(r.hunk_count == 2); /* two separate hunks */
   }

   /* --- diff_format_summary --- */
   {
      diff_result_t r;
      memset(&r, 0, sizeof(r));
      r.additions = 5;
      r.deletions = 3;
      r.hunk_count = 2;

      char *s = diff_format_summary(&r);
      assert(s != NULL);
      assert(strstr(s, "+5") != NULL);
      assert(strstr(s, "-3") != NULL);
      assert(strstr(s, "2 hunk") != NULL);
      free(s);
   }

   /* --- diff_format_summary no changes --- */
   {
      diff_result_t r;
      memset(&r, 0, sizeof(r));
      char *s = diff_format_summary(&r);
      assert(s != NULL);
      assert(strcmp(s, "no changes") == 0);
      free(s);
   }

   /* --- diff_format_unified basic --- */
   {
      diff_result_t r;
      int rc = diff_compute("aaa\nbbb\nccc\n", "aaa\nBBB\nccc\n", &r);
      assert(rc == 0);

      char *u = diff_format_unified("aaa\nbbb\nccc\n", "aaa\nBBB\nccc\n", &r);
      assert(u != NULL);
      assert(strstr(u, "@@") != NULL);
      assert(strstr(u, "-bbb") != NULL);
      assert(strstr(u, "+BBB") != NULL);
      free(u);
   }

   /* --- diff_format_unified new file --- */
   {
      diff_result_t r;
      int rc = diff_compute("", "hello\nworld\n", &r);
      assert(rc == 0);

      char *u = diff_format_unified("", "hello\nworld\n", &r);
      assert(u != NULL);
      assert(strstr(u, "+hello") != NULL);
      assert(strstr(u, "+world") != NULL);
      free(u);
   }

   /* --- diff_result_to_json --- */
   {
      diff_result_t r;
      memset(&r, 0, sizeof(r));
      r.additions = 3;
      r.deletions = 1;
      r.hunk_count = 1;
      r.hunks[0].old_start = 5;
      r.hunks[0].old_count = 3;
      r.hunks[0].new_start = 5;
      r.hunks[0].new_count = 5;
      r.hunks[0].additions = 3;
      r.hunks[0].deletions = 1;

      cJSON *j = diff_result_to_json(&r);
      assert(j != NULL);
      assert(cJSON_GetObjectItem(j, "additions")->valuedouble == 3.0);
      assert(cJSON_GetObjectItem(j, "deletions")->valuedouble == 1.0);
      assert(cJSON_GetObjectItem(j, "hunk_count")->valuedouble == 1.0);
      assert(cJSON_IsFalse(cJSON_GetObjectItem(j, "truncated")));

      cJSON *hunks = cJSON_GetObjectItem(j, "hunks");
      assert(cJSON_IsArray(hunks));
      assert(cJSON_GetArraySize(hunks) == 1);

      cJSON *h0 = cJSON_GetArrayItem(hunks, 0);
      assert(cJSON_GetObjectItem(h0, "old_start")->valuedouble == 5.0);
      assert(cJSON_GetObjectItem(h0, "additions")->valuedouble == 3.0);
      cJSON_Delete(j);
   }

   /* --- format_unified empty result --- */
   {
      diff_result_t r;
      memset(&r, 0, sizeof(r));
      char *u = diff_format_unified("a\n", "a\n", &r);
      assert(u != NULL);
      assert(strlen(u) == 0);
      free(u);
   }

   printf("all tests passed\n");
   return 0;
}
