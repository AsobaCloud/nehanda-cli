/* test_history.c: unit tests for the chat history module */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "aimee.h"
#include "history.h"
#include "platform_test_util.h"

int main(void)
{
   printf("history: ");

   /* Use isolated temp HOME */
   char tmpdir[512];
   snprintf(tmpdir, sizeof(tmpdir), "%s/aimee-test-history-XXXXXX", platform_tmpdir());
   assert(platform_mkdtemp(tmpdir) != NULL);
   platform_setenv("HOME", tmpdir);

   char hpath[512];
   snprintf(hpath, sizeof(hpath), "%s/history.jsonl", tmpdir);

   /* --- history_open / history_close: basic lifecycle --- */
   {
      chat_history_t *h = history_open(hpath, 0);
      assert(h != NULL);
      assert(history_count(h) == 0);
      history_close(h);
   }

   /* --- history_add: basic add and count --- */
   {
      chat_history_t *h = history_open(hpath, 0);
      assert(h != NULL);
      history_add(h, "hello");
      history_add(h, "world");
      assert(history_count(h) == 2);
      history_close(h);

      /* Cleanup for next test */
      unlink(hpath);
   }

   /* --- history_add: consecutive duplicate suppression --- */
   {
      chat_history_t *h = history_open(hpath, 0);
      assert(h != NULL);
      history_add(h, "same");
      history_add(h, "same"); /* duplicate: should be suppressed */
      history_add(h, "same"); /* duplicate: should be suppressed */
      assert(history_count(h) == 1);

      /* Non-consecutive duplicates are kept */
      history_add(h, "other");
      history_add(h, "same"); /* not consecutive: should be kept */
      assert(history_count(h) == 3);
      history_close(h);

      unlink(hpath);
   }

   /* --- history_add: slash commands excluded --- */
   {
      chat_history_t *h = history_open(hpath, 0);
      assert(h != NULL);
      history_add(h, "/help");
      history_add(h, "/quit");
      history_add(h, "/status");
      assert(history_count(h) == 0);

      /* Normal entries still work */
      history_add(h, "normal entry");
      assert(history_count(h) == 1);
      history_close(h);

      unlink(hpath);
   }

   /* --- history_add: cap enforcement --- */
   {
      int max = 5;
      chat_history_t *h = history_open(hpath, max);
      assert(h != NULL);

      /* Add more entries than the cap */
      history_add(h, "entry1");
      history_add(h, "entry2");
      history_add(h, "entry3");
      history_add(h, "entry4");
      history_add(h, "entry5");
      history_add(h, "entry6"); /* exceeds cap */
      history_add(h, "entry7"); /* exceeds cap */

      assert(history_count(h) <= max);
      history_close(h);

      unlink(hpath);
   }

   /* --- history_prev / history_next navigation --- */
   {
      chat_history_t *h = history_open(hpath, 0);
      assert(h != NULL);
      history_add(h, "first");
      history_add(h, "second");
      history_add(h, "third");
      assert(history_count(h) == 3);

      /* history_next at start (no navigation yet) returns NULL */
      const char *n = history_next(h);
      assert(n == NULL);

      /* history_prev walks backwards through entries (newest first) */
      const char *p = history_prev(h);
      assert(p != NULL);
      assert(strcmp(p, "third") == 0);

      p = history_prev(h);
      assert(p != NULL);
      assert(strcmp(p, "second") == 0);

      p = history_prev(h);
      assert(p != NULL);
      assert(strcmp(p, "first") == 0);

      /* At oldest: history_prev returns NULL */
      p = history_prev(h);
      assert(p == NULL);

      /* history_next walks forward */
      n = history_next(h);
      assert(n != NULL);
      assert(strcmp(n, "second") == 0);

      n = history_next(h);
      assert(n != NULL);
      assert(strcmp(n, "third") == 0);

      /* Past newest: history_next returns NULL (back at current input) */
      n = history_next(h);
      assert(n == NULL);

      history_close(h);

      unlink(hpath);
   }

   /* --- history_reset_nav --- */
   {
      chat_history_t *h = history_open(hpath, 0);
      assert(h != NULL);
      history_add(h, "alpha");
      history_add(h, "beta");
      history_add(h, "gamma");

      /* Navigate back a couple steps */
      history_prev(h); /* gamma */
      history_prev(h); /* beta */

      /* Reset navigation */
      history_reset_nav(h);

      /* After reset, history_next should return NULL (at current position) */
      const char *n = history_next(h);
      assert(n == NULL);

      /* And history_prev should start from the newest again */
      const char *p = history_prev(h);
      assert(p != NULL);
      assert(strcmp(p, "gamma") == 0);

      history_close(h);

      unlink(hpath);
   }

   /* --- Persistence: write, close, reopen, verify entries loaded --- */
   {
      /* Write entries and close */
      {
         chat_history_t *h = history_open(hpath, 0);
         assert(h != NULL);
         history_add(h, "persist_one");
         history_add(h, "persist_two");
         history_add(h, "persist_three");
         assert(history_count(h) == 3);
         history_close(h);
      }

      /* Reopen and verify entries are loaded */
      {
         chat_history_t *h = history_open(hpath, 0);
         assert(h != NULL);
         assert(history_count(h) == 3);

         /* Navigate to confirm the actual content round-tripped */
         const char *p = history_prev(h);
         assert(p != NULL);
         assert(strcmp(p, "persist_three") == 0);

         p = history_prev(h);
         assert(p != NULL);
         assert(strcmp(p, "persist_two") == 0);

         p = history_prev(h);
         assert(p != NULL);
         assert(strcmp(p, "persist_one") == 0);

         history_close(h);
      }

      unlink(hpath);
   }

   /* --- history_set_completions: basic API --- */
   {
      chat_history_t *h = history_open(hpath, 0);
      assert(h != NULL);

      /* Initially no completions; vim mode off. */
      assert(history_vim_mode(h) == 0);

      const char *words[] = {"help", "status", "model", "clear", "quit"};
      history_set_completions(h, words, 5);

      /* history_vim_mode should remain off. */
      assert(history_vim_mode(h) == 0);

      history_close(h);
      unlink(hpath);
   }

   /* --- history_set_vim_mode: toggle --- */
   {
      chat_history_t *h = history_open(hpath, 0);
      assert(h != NULL);

      assert(history_vim_mode(h) == 0);

      history_set_vim_mode(h, 1);
      assert(history_vim_mode(h) == 1);

      history_set_vim_mode(h, 0);
      assert(history_vim_mode(h) == 0);

      history_set_vim_mode(h, 1);
      assert(history_vim_mode(h) != 0);

      history_close(h);
      unlink(hpath);
   }

   /* --- history_set_completions: NULL / zero clears completions --- */
   {
      chat_history_t *h = history_open(hpath, 0);
      assert(h != NULL);

      const char *words[] = {"foo", "bar"};
      history_set_completions(h, words, 2);

      /* Clear by passing NULL. */
      history_set_completions(h, NULL, 0);

      history_close(h);
      unlink(hpath);
   }

   /* Cleanup */
   char cmd[512];
   snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
   (void)system(cmd);

   printf("all tests passed\n");
   return 0;
}
