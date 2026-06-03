#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "db1.h"
#include <sqlite3.h>

/* Private to src/db1/, but this test verifies child-table cascades directly. */
extern sqlite3 *db1_conn(void);

static int fetch_count(const char *sql)
{
   sqlite3_stmt *stmt = NULL;
   assert(sqlite3_prepare_v2(db1_conn(), sql, -1, &stmt, NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   int n = sqlite3_column_int(stmt, 0);
   sqlite3_finalize(stmt);
   return n;
}

static void fetch_text(char *out, size_t out_sz, const char *sql)
{
   sqlite3_stmt *stmt = NULL;
   assert(sqlite3_prepare_v2(db1_conn(), sql, -1, &stmt, NULL) == SQLITE_OK);
   assert(sqlite3_step(stmt) == SQLITE_ROW);
   const unsigned char *txt = sqlite3_column_text(stmt, 0);
   snprintf(out, out_sz, "%s", txt ? (const char *)txt : "");
   sqlite3_finalize(stmt);
}

int main(void)
{
   assert(db1_init(":memory:") == 0);

   int count = -1;
   int max_seq = -1;
   assert(db1_windows_session_scan_state("sess-1", &count, &max_seq) == 0);
   assert(count == 0);
   assert(max_seq == 0);

   int64_t w1 = db1_window_create_raw("sess-1", 1, "first", "2026-01-01 00:00:00");
   int64_t w2 = db1_window_create_raw("sess-1", 2, "second", "2026-01-01 00:00:01");
   int64_t w3 = db1_window_create_raw("sess-2", 1, "other", "2026-01-01 00:00:02");
   assert(w1 > 0);
   assert(w2 > 0);
   assert(w3 > 0);

   assert(db1_window_add_term(w1, "alpha") == 0);
   assert(db1_window_add_term(w2, "beta") == 0);
   assert(db1_window_add_file(w1, "src/a.c") == 0);
   assert(db1_window_add_file(w2, "src/b.c") == 0);

   assert(db1_windows_session_scan_state("sess-1", &count, &max_seq) == 0);
   assert(count == 2);
   assert(max_seq == 2);
   char session_id[128];
   assert(db1_window_session_id(w1, session_id, sizeof(session_id)) == 1);
   assert(strcmp(session_id, "sess-1") == 0);

   assert(fetch_count("SELECT COUNT(*) FROM window_terms") == 2);
   assert(fetch_count("SELECT COUNT(*) FROM window_files") == 2);

   assert(db1_windows_delete_after_turn("sess-1", 1) == 1);
   assert(db1_windows_session_scan_state("sess-1", &count, &max_seq) == 0);
   assert(count == 1);
   assert(max_seq == 1);
   assert(fetch_count("SELECT COUNT(*) FROM window_terms") == 1);
   assert(fetch_count("SELECT COUNT(*) FROM window_files") == 1);
   assert(fetch_count("SELECT COUNT(*) FROM windows WHERE session_id = 'sess-2'") == 1);

   {
      const char *terms[] = {"alpha"};
      db1_window_search_candidate_t candidates[4];
      memset(candidates, 0, sizeof(candidates));
      assert(db1_windows_find_candidates_by_terms(terms, 1, candidates, 4) == 1);
      assert(candidates[0].window_id == w1);
      assert(strcmp(candidates[0].session_id, "sess-1") == 0);
      assert(candidates[0].seq == 1);
      assert(strcmp(candidates[0].summary, "first") == 0);
      assert(candidates[0].match_count == 1);

      char files[4][MAX_PATH_LEN];
      memset(files, 0, sizeof(files));
      assert(db1_window_list_files(w1, files, 4) == 1);
      assert(strcmp(files[0], "src/a.c") == 0);

      db1_window_lexical_hit_t hits[4];
      memset(hits, 0, sizeof(hits));
      assert(db1_window_index_summary(w1, "first alpha window") == 0);
      assert(db1_windows_find_lexical_hits(terms, 1, hits, 4) == 1);
      assert(hits[0].window_id == w1);
   }

   {
      int64_t compact = db1_window_create_raw("sess-3", 1, "compact", "2026-01-01 00:00:03");
      assert(compact > 0);
      assert(db1_window_add_term(compact, "a") == 0);
      assert(db1_window_add_term(compact, "alphabet") == 0);
      assert(db1_window_add_term(compact, "mid") == 0);
      assert(db1_window_prune_terms_keep_top(compact, 1) == 0);
      assert(db1_window_set_tier(compact, "summary") == 0);
      char term[32];
      fetch_text(term, sizeof(term),
                 "SELECT term FROM window_terms WHERE window_id = "
                 "(SELECT id FROM windows WHERE session_id = 'sess-3')");
      assert(strcmp(term, "alphabet") == 0);

      assert(db1_window_add_file(compact, "a") == 0);
      assert(db1_window_add_file(compact, "b") == 0);
      assert(db1_window_add_file(compact, "c") == 0);
      assert(db1_window_prune_files_keep_top(compact, 2) == 0);
      assert(fetch_count("SELECT COUNT(*) FROM window_files WHERE window_id = "
                         "(SELECT id FROM windows WHERE session_id = 'sess-3')") == 2);
      assert(db1_window_delete_all_files(compact) == 0);
      assert(fetch_count("SELECT COUNT(*) FROM window_files WHERE window_id = "
                         "(SELECT id FROM windows WHERE session_id = 'sess-3')") == 0);

      int64_t ids[8];
      int listed = db1_windows_list_ids_by_tier_before_days("summary", 30, ids, 8);
      assert(listed >= 1);
   }

   db1_shutdown();
   printf("test_windows: ok\n");
   return 0;
}
