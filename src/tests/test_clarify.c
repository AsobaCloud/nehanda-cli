/* test_clarify.c: unit tests for the planning-preparation clarification subsystem */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "db1.h"

/* --- Helpers --- */

static void setup(void)
{
   assert(db1_init(":memory:") == 0);
}

static void teardown(void)
{
   db1_shutdown();
}

/* --- Tests: clarify_start --- */

static void test_start_creates_session(void)
{
   setup();

   clarify_session_t s;
   int id = db1_clarify_start("Add a search feature to the app", &s);
   assert(id > 0);
   assert(s.id == id);
   assert(strcmp(s.description, "Add a search feature to the app") == 0);
   assert(s.status == CLARIFY_OPEN);
   assert(s.score >= 0.0f && s.score <= 1.0f);
   /* Should have generated the first question */
   assert(s.qa_count == 1);
   assert(s.qa[0].question[0] != '\0');
   assert(s.qa[0].dimension[0] != '\0');
   assert(s.qa[0].answered == 0);

   teardown();
}

static void test_start_rejects_empty_description(void)
{
   setup();

   clarify_session_t s;
   int id = db1_clarify_start("", &s);
   assert(id == -1);

   id = db1_clarify_start(NULL, &s);
   assert(id == -1);

   teardown();
}

static void test_start_rejects_description_too_long(void)
{
   setup();

   char long_desc[CLARIFY_DESC_LEN + 8];
   memset(long_desc, 'x', sizeof(long_desc) - 1);
   long_desc[sizeof(long_desc) - 1] = '\0';

   clarify_session_t s;
   int id = db1_clarify_start(long_desc, &s);
   assert(id == -1);

   teardown();
}

/* --- Tests: clarify_get --- */

static void test_get_returns_session(void)
{
   setup();

   clarify_session_t s;
   int id = db1_clarify_start("Rewrite the login flow", &s);
   assert(id > 0);

   clarify_session_t loaded;
   int rc = db1_clarify_get(id, &loaded);
   assert(rc == 0);
   assert(loaded.id == id);
   assert(strcmp(loaded.description, "Rewrite the login flow") == 0);
   assert(loaded.status == CLARIFY_OPEN);
   assert(loaded.qa_count == s.qa_count);

   teardown();
}

static void test_get_nonexistent_returns_error(void)
{
   setup();

   clarify_session_t s;
   int rc = db1_clarify_get(9999, &s);
   assert(rc == -1);

   teardown();
}

/* --- Tests: clarify_score --- */

static void test_score_increases_with_description_length(void)
{
   clarify_session_t s;
   memset(&s, 0, sizeof(s));

   /* Short description */
   snprintf(s.description, sizeof(s.description), "fix bug");
   float score_short = db1_clarify_score(&s);

   /* Medium description (>= 80 chars) */
   snprintf(s.description, sizeof(s.description),
            "Fix the authentication bug that causes users to be logged out unexpectedly "
            "when the session token expires.");
   float score_medium = db1_clarify_score(&s);

   /* Long description */
   char long_desc[256];
   memset(long_desc, 'a', sizeof(long_desc) - 1);
   long_desc[sizeof(long_desc) - 1] = '\0';
   snprintf(s.description, sizeof(s.description), "%s", long_desc);
   float score_long = db1_clarify_score(&s);

   assert(score_short < score_medium);
   assert(score_medium <= score_long);
   (void)score_long;

   teardown();
}

static void test_score_increases_with_answered_questions(void)
{
   setup();

   clarify_session_t s;
   int id = db1_clarify_start("Refactor the database layer", &s);
   assert(id > 0);
   float score_before = s.score;

   /* Answer the first question */
   clarify_session_t s2;
   int rc = db1_clarify_answer(id, "Only the query builder, not the schema layer.", &s2);
   assert(rc == 0);
   float score_after = s2.score;

   assert(score_after > score_before);

   teardown();
}

static void test_score_null_returns_zero(void)
{
   float s = db1_clarify_score(NULL);
   assert(s == 0.0f);
}

/* --- Tests: clarify_answer --- */

static void test_answer_records_and_advances(void)
{
   setup();

   clarify_session_t s;
   int id = db1_clarify_start("Add user notifications", &s);
   assert(id > 0);
   assert(s.qa_count == 1);
   assert(s.qa[0].answered == 0);

   clarify_session_t s2;
   int rc = db1_clarify_answer(id, "Email and in-app banners, not SMS.", &s2);
   assert(rc == 0);
   /* First Q should now be answered */
   int found_answered = 0;
   for (int i = 0; i < s2.qa_count; i++)
      if (s2.qa[i].answered)
         found_answered++;
   assert(found_answered >= 1);

   teardown();
}

static void test_answer_rejects_nonexistent_session(void)
{
   setup();

   clarify_session_t s;
   int rc = db1_clarify_answer(9999, "some answer", &s);
   assert(rc == -1);

   teardown();
}

static void test_answer_rejects_empty_answer(void)
{
   setup();

   clarify_session_t s;
   int id = db1_clarify_start("Create a dashboard for the admin panel", &s);
   assert(id > 0);

   clarify_session_t s2;
   int rc = db1_clarify_answer(id, "", &s2);
   assert(rc == -1);

   teardown();
}

static void test_answer_rejects_ready_session(void)
{
   setup();

   /* Create a session that's already ready by repeatedly answering */
   clarify_session_t s;
   int id = db1_clarify_start("Add full-text search with ranking to the knowledge base module", &s);
   assert(id > 0);

   /* Answer questions until ready */
   int rounds = 0;
   while (s.status == CLARIFY_OPEN && rounds < CLARIFY_MAX_QA)
   {
      int rc = db1_clarify_answer(id, "Confirmed, no further constraints.", &s);
      if (rc != 0)
         break;
      rounds++;
   }

   if (s.status == CLARIFY_READY)
   {
      clarify_session_t s2;
      int rc = db1_clarify_answer(id, "Another answer", &s2);
      assert(rc == -1); /* should reject since session is ready */
   }

   teardown();
}

/* --- Tests: ready state and spec crystallisation --- */

static void test_session_reaches_ready_state(void)
{
   setup();

   clarify_session_t s;
   /* Use a long description to start with a higher base score */
   int id = db1_clarify_start(
       "Implement a rate-limiting middleware for the REST API that tracks requests "
       "per IP address and returns 429 when thresholds are exceeded",
       &s);
   assert(id > 0);

   /* Answer questions until ready or we exhaust rounds */
   int rounds = 0;
   while (s.status == CLARIFY_OPEN && rounds < CLARIFY_MAX_QA)
   {
      int rc = db1_clarify_answer(id, "Yes, default configuration is fine.", &s);
      if (rc != 0)
         break;
      rounds++;
   }

   /* Should eventually reach ready */
   assert(s.status == CLARIFY_READY);
   assert(s.spec[0] != '\0');
   assert(strstr(s.spec, "Task Specification") != NULL);
   assert(strstr(s.spec, s.description) != NULL);

   teardown();
}

/* --- Tests: clarify_next_question --- */

static void test_next_question_returns_question(void)
{
   setup();

   clarify_session_t s;
   int id = db1_clarify_start("Build an API client library", &s);
   assert(id > 0);
   (void)id;

   char q[CLARIFY_TEXT_LEN], dim[CLARIFY_DIM_NAME_LEN];
   int rc = db1_clarify_next_question(&s, q, sizeof(q), dim, sizeof(dim));
   assert(rc == 0);
   assert(q[0] != '\0');
   assert(dim[0] != '\0');

   teardown();
}

static void test_next_question_ready_returns_one(void)
{
   clarify_session_t s;
   memset(&s, 0, sizeof(s));
   s.status = CLARIFY_READY;
   s.score = 1.0f;
   snprintf(s.description, sizeof(s.description), "done");

   char q[CLARIFY_TEXT_LEN], dim[CLARIFY_DIM_NAME_LEN];
   int rc = db1_clarify_next_question(&s, q, sizeof(q), dim, sizeof(dim));
   assert(rc == 1); /* already ready */
}

/* --- Tests: clarify_crystallize --- */

static void test_crystallize_includes_description(void)
{
   clarify_session_t s;
   memset(&s, 0, sizeof(s));
   snprintf(s.description, sizeof(s.description), "Rebuild the payment processor");

   char *spec = db1_clarify_crystallize(&s);
   assert(spec != NULL);
   assert(strstr(spec, "Rebuild the payment processor") != NULL);
   assert(strstr(spec, "Task Specification") != NULL);
   free(spec);
}

static void test_crystallize_includes_answered_qa(void)
{
   clarify_session_t s;
   memset(&s, 0, sizeof(s));
   snprintf(s.description, sizeof(s.description), "Add logging");

   s.qa_count = 1;
   snprintf(s.qa[0].dimension, sizeof(s.qa[0].dimension), "scope");
   snprintf(s.qa[0].question, sizeof(s.qa[0].question), "What files should be logged?");
   snprintf(s.qa[0].answer, sizeof(s.qa[0].answer), "Only error events, not debug.");
   s.qa[0].answered = 1;

   char *spec = db1_clarify_crystallize(&s);
   assert(spec != NULL);
   assert(strstr(spec, "Only error events") != NULL);
   free(spec);
}

static void test_crystallize_skips_unanswered_qa(void)
{
   clarify_session_t s;
   memset(&s, 0, sizeof(s));
   snprintf(s.description, sizeof(s.description), "Add logging");

   s.qa_count = 1;
   snprintf(s.qa[0].dimension, sizeof(s.qa[0].dimension), "scope");
   snprintf(s.qa[0].question, sizeof(s.qa[0].question), "What files should be logged?");
   s.qa[0].answer[0] = '\0';
   s.qa[0].answered = 0;

   char *spec = db1_clarify_crystallize(&s);
   assert(spec != NULL);
   /* Unanswered Q should not appear in spec */
   assert(strstr(spec, "What files should be logged?") == NULL);
   free(spec);
}

/* --- Tests: clarify_to_json --- */

static void test_to_json_produces_valid_json(void)
{
   clarify_session_t s;
   memset(&s, 0, sizeof(s));
   s.id = 42;
   s.status = CLARIFY_OPEN;
   s.score = 0.35f;
   snprintf(s.description, sizeof(s.description), "Refactor auth module");
   snprintf(s.created_at, sizeof(s.created_at), "2025-01-01T00:00:00Z");
   snprintf(s.updated_at, sizeof(s.updated_at), "2025-01-01T00:00:00Z");

   char *json = db1_clarify_to_json(&s);
   assert(json != NULL);
   assert(strstr(json, "\"id\":42") != NULL);
   assert(strstr(json, "\"status\":\"open\"") != NULL);
   assert(strstr(json, "\"description\":\"Refactor auth module\"") != NULL);
   free(json);
}

static void test_to_json_ready_status(void)
{
   clarify_session_t s;
   memset(&s, 0, sizeof(s));
   s.id = 7;
   s.status = CLARIFY_READY;
   s.score = 0.85f;
   snprintf(s.description, sizeof(s.description), "Build dashboard");
   snprintf(s.created_at, sizeof(s.created_at), "2025-01-01T00:00:00Z");
   snprintf(s.updated_at, sizeof(s.updated_at), "2025-01-01T00:00:00Z");

   char *json = db1_clarify_to_json(&s);
   assert(json != NULL);
   assert(strstr(json, "\"status\":\"ready\"") != NULL);
   free(json);
}

static void test_to_json_null_returns_null(void)
{
   char *json = db1_clarify_to_json(NULL);
   assert(json == NULL);
}

/* --- Tests: weakest dimension selection --- */

static void test_weakest_dim_returns_unanswered(void)
{
   clarify_session_t s;
   memset(&s, 0, sizeof(s));
   snprintf(s.description, sizeof(s.description), "Add search");

   /* Mark scope as answered */
   s.qa_count = 1;
   snprintf(s.qa[0].dimension, sizeof(s.qa[0].dimension), "scope");
   snprintf(s.qa[0].question, sizeof(s.qa[0].question), "Scope?");
   snprintf(s.qa[0].answer, sizeof(s.qa[0].answer), "All pages.");
   s.qa[0].answered = 1;
   s.qa[0].seq = 0;

   char weakest[CLARIFY_DIM_NAME_LEN];
   db1_clarify_weakest_dim(&s, weakest, sizeof(weakest));
   /* weakest should not be "scope" since that's answered */
   assert(strcmp(weakest, "scope") != 0);
}

int main(void)
{
   printf("clarify: ");

   test_start_creates_session();
   test_start_rejects_empty_description();
   test_start_rejects_description_too_long();
   test_get_returns_session();
   test_get_nonexistent_returns_error();
   test_score_increases_with_description_length();
   test_score_increases_with_answered_questions();
   test_score_null_returns_zero();
   test_answer_records_and_advances();
   test_answer_rejects_nonexistent_session();
   test_answer_rejects_empty_answer();
   test_answer_rejects_ready_session();
   test_session_reaches_ready_state();
   test_next_question_returns_question();
   test_next_question_ready_returns_one();
   test_crystallize_includes_description();
   test_crystallize_includes_answered_qa();
   test_crystallize_skips_unanswered_qa();
   test_to_json_produces_valid_json();
   test_to_json_ready_status();
   test_to_json_null_returns_null();
   test_weakest_dim_returns_unanswered();

   printf("all tests passed\n");
   return 0;
}
