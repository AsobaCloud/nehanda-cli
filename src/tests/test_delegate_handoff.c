/* test_delegate_handoff.c: structured delegate handoff validation tests */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cmd_agent_delegate_impl.h"

static const char *valid_handoff =
    "{"
    "\"schema_version\":\"delegate_result_v1\","
    "\"status\":\"done\","
    "\"packet_id\":\"packet-one\","
    "\"changed_files\":[\"src/foo.c\"],"
    "\"owned_files_touched\":true,"
    "\"outside_ownership_touches\":[],"
    "\"commands_run\":[{\"command\":\"make -C src test\",\"exit_code\":0,"
    "\"summary\":\"built focused test\"}],"
    "\"tests\":[{\"name\":\"unit-test-foo\",\"status\":\"passed\","
    "\"command\":\"./src/build/obj/tests/unit-test-foo\"}],"
    "\"risks\":[],"
    "\"supervisor_actions\":[],"
    "\"summary\":\"Implemented focused change.\""
    "}";

static void test_valid_handoff_parses(void)
{
   delegate_handoff_validation_t v;
   assert(delegate_handoff_validate_text(valid_handoff, "[\"src/foo.c\"]", 1, &v) == 0);
   assert(v.valid == 1);
   assert(strcmp(v.status, "done") == 0);
   assert(strcmp(v.raw_status, "done") == 0);
   assert(v.changed_files_count == 1);
   assert(v.commands_run == 1);
   assert(v.passed_tests == 1);
   assert(v.outside_ownership_count == 0);
   assert(v.needs_supervisor_review == 0);
   printf("  PASS: test_valid_handoff_parses\n");
}

static void test_invalid_json_rejected(void)
{
   delegate_handoff_validation_t v;
   assert(delegate_handoff_validate_text("not json", NULL, 1, &v) != 0);
   assert(v.valid == 0);
   assert(strcmp(v.status, "needs_supervisor_review") == 0);
   assert(strstr(v.error, "valid JSON object") != NULL);
   printf("  PASS: test_invalid_json_rejected\n");
}

static void test_missing_required_fields_rejected(void)
{
   const char *json = "{\"schema_version\":\"delegate_result_v1\",\"status\":\"done\"}";
   delegate_handoff_validation_t v;
   assert(delegate_handoff_validate_text(json, NULL, 1, &v) != 0);
   assert(v.valid == 0);
   assert(strcmp(v.status, "needs_supervisor_review") == 0);
   assert(strstr(v.error, "required fields") != NULL);
   printf("  PASS: test_missing_required_fields_rejected\n");
}

static void test_supervisor_actions_optional(void)
{
   const char *json = "{"
                      "\"schema_version\":\"delegate_result_v1\","
                      "\"status\":\"done\","
                      "\"changed_files\":[\"src/foo.c\"],"
                      "\"tests\":[{\"name\":\"unit-test-foo\",\"status\":\"passed\"}],"
                      "\"summary\":\"Implemented focused change.\""
                      "}";
   delegate_handoff_validation_t v;
   assert(delegate_handoff_validate_text(json, "[\"src/foo.c\"]", 1, &v) == 0);
   assert(v.valid == 1);
   assert(strcmp(v.status, "done") == 0);
   assert(v.passed_tests == 1);
   printf("  PASS: test_supervisor_actions_optional\n");
}

static void test_empty_summary_rejected(void)
{
   const char *json = "{"
                      "\"schema_version\":\"delegate_result_v1\","
                      "\"status\":\"done\","
                      "\"changed_files\":[],"
                      "\"tests\":[],"
                      "\"supervisor_actions\":[],"
                      "\"summary\":\"   \""
                      "}";
   delegate_handoff_validation_t v;
   assert(delegate_handoff_validate_text(json, NULL, 1, &v) != 0);
   assert(v.valid == 0);
   assert(strstr(v.error, "required fields") != NULL);
   printf("  PASS: test_empty_summary_rejected\n");
}

static void test_done_without_verification_downgrades(void)
{
   const char *json = "{"
                      "\"schema_version\":\"delegate_result_v1\","
                      "\"status\":\"done\","
                      "\"changed_files\":[\"src/foo.c\"],"
                      "\"tests\":[],"
                      "\"supervisor_actions\":[],"
                      "\"summary\":\"No verification was run.\""
                      "}";
   delegate_handoff_validation_t v;
   assert(delegate_handoff_validate_text(json, "[\"src/foo.c\"]", 1, &v) == 0);
   assert(v.valid == 1);
   assert(strcmp(v.raw_status, "done") == 0);
   assert(strcmp(v.status, "partial") == 0);
   assert(v.done_without_verification == 1);
   assert(strstr(v.error, "downgraded") != NULL);

   assert(delegate_handoff_validate_text(json, "[\"src/foo.c\"]", 0, &v) == 0);
   assert(strcmp(v.status, "done") == 0);
   assert(v.done_without_verification == 0);
   printf("  PASS: test_done_without_verification_downgrades\n");
}

static void test_outside_owned_files_need_review(void)
{
   const char *json = "{"
                      "\"schema_version\":\"delegate_result_v1\","
                      "\"status\":\"done\","
                      "\"changed_files\":[\"src/foo.c\",\"src/bar.c\"],"
                      "\"outside_ownership_touches\":[],"
                      "\"tests\":[{\"name\":\"unit-test-foo\",\"status\":\"passed\"}],"
                      "\"supervisor_actions\":[],"
                      "\"summary\":\"Touched one extra file.\""
                      "}";
   delegate_handoff_validation_t v;
   assert(delegate_handoff_validate_text(json, "[\"src/foo.c\"]", 1, &v) == 0);
   assert(v.valid == 1);
   assert(strcmp(v.status, "needs_supervisor_review") == 0);
   assert(v.needs_supervisor_review == 1);
   assert(v.outside_ownership_count == 1);
   assert(strstr(v.error, "outside owned_files") != NULL);
   printf("  PASS: test_outside_owned_files_need_review\n");
}

static void test_prompt_contract_helpers(void)
{
   char *prompt = delegate_handoff_append_contract("Implement packet.", "packet-alpha");
   assert(prompt != NULL);
   assert(strstr(prompt, "delegate_result_v1") != NULL);
   assert(strstr(prompt, "packet-alpha") != NULL);
   free(prompt);

   char *repair = delegate_handoff_repair_prompt("bad response", "missing schema");
   assert(repair != NULL);
   assert(strstr(repair, "Repair it now") != NULL);
   assert(strstr(repair, "missing schema") != NULL);
   assert(strstr(repair, "bad response") != NULL);
   free(repair);
   printf("  PASS: test_prompt_contract_helpers\n");
}

static void test_validation_json_fields(void)
{
   delegate_handoff_validation_t v;
   assert(delegate_handoff_validate_text(valid_handoff, "[\"src/foo.c\"]", 1, &v) == 0);
   cJSON *obj = cJSON_CreateObject();
   delegate_handoff_add_validation_json(obj, &v);
   assert(cJSON_IsTrue(cJSON_GetObjectItem(obj, "handoff_valid")));
   assert(strcmp(cJSON_GetObjectItem(obj, "handoff_status")->valuestring, "done") == 0);
   assert(cJSON_GetObjectItem(obj, "handoff_passed_tests")->valueint == 1);
   assert(cJSON_GetObjectItem(obj, "handoff_changed_files")->valueint == 1);
   cJSON_Delete(obj);
   printf("  PASS: test_validation_json_fields\n");
}

int main(void)
{
   test_valid_handoff_parses();
   test_invalid_json_rejected();
   test_missing_required_fields_rejected();
   test_supervisor_actions_optional();
   test_empty_summary_rejected();
   test_done_without_verification_downgrades();
   test_outside_owned_files_need_review();
   test_prompt_contract_helpers();
   test_validation_json_fields();
   printf("delegate_handoff: all tests passed\n");
   return 0;
}
