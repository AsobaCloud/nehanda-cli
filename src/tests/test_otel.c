/* test_otel.c: unit tests for OpenTelemetry trace export (otel.c) */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "otel.h"

/* ------------------------------------------------------------------ helpers */

static void test_disabled_by_default(void)
{
   /* Without otel_init, export must be disabled */
   assert(!otel_is_enabled());

   /* All operations are no-ops when disabled */
   otel_span_t span;
   otel_span_start(&span, "test", NULL);
   otel_span_attr_s(&span, "key", "val");
   otel_span_attr_i(&span, "n", 42);
   otel_span_end(&span, 0);
   otel_flush(); /* must not crash */

   otel_span_t deleg;
   otel_delegation_start(&deleg, "reviewer", "fix the bug");
   otel_delegation_end(&deleg, 0);

   printf("  disabled_by_default: ok\n");
}

static void test_init_enables_export(void)
{
   /* After init with a non-empty endpoint, should be enabled */
   otel_init("http://127.0.0.1:4318", "aimee-test", "test-session-0001");
   assert(otel_is_enabled());
   printf("  init_enables_export: ok\n");
}

static void test_init_empty_disables(void)
{
   otel_init("http://127.0.0.1:4318", "aimee-test", "test-session-0001");
   assert(otel_is_enabled());

   otel_init("", NULL, NULL);
   assert(!otel_is_enabled());
   printf("  init_empty_disables: ok\n");
}

static void test_span_start_fills_ids(void)
{
   otel_init("http://127.0.0.1:4318", "aimee-test", "aabbccdd11223344aabbccdd11223344");

   otel_span_t s;
   otel_span_start(&s, "agent.turn", NULL);

   /* trace_id should be populated (derived from session_id) */
   assert(s.trace_id[0] != '\0');
   assert(strlen(s.trace_id) == 32);

   /* span_id should be populated */
   assert(s.span_id[0] != '\0');
   assert(strlen(s.span_id) == 16);

   /* no parent */
   assert(s.parent_span_id[0] == '\0');

   /* name */
   assert(strcmp(s.name, "agent.turn") == 0);

   /* start_ns should be non-zero */
   assert(s.start_ns > 0);

   printf("  span_start_fills_ids: ok\n");
}

static void test_span_with_parent(void)
{
   otel_init("http://127.0.0.1:4318", "aimee", "test-session-aaaa");

   otel_span_t parent;
   otel_span_start(&parent, "agent.turn", NULL);

   otel_span_t child;
   otel_span_start(&child, "tool_call", parent.span_id);

   assert(strcmp(child.parent_span_id, parent.span_id) == 0);
   assert(strcmp(child.trace_id, parent.trace_id) == 0);

   printf("  span_with_parent: ok\n");
}

static void test_span_attributes(void)
{
   otel_init("http://127.0.0.1:4318", "aimee", "session-attr-test");

   otel_span_t s;
   otel_span_start(&s, "tool_call", NULL);

   otel_span_attr_s(&s, "tool.name", "bash");
   otel_span_attr_s(&s, "tool.args", "ls -la");
   otel_span_attr_i(&s, "turn", 3);

   assert(s.attr_count == 3);
   assert(strcmp(s.attrs[0].k, "tool.name") == 0);
   assert(strcmp(s.attrs[0].v, "bash") == 0);
   assert(strcmp(s.attrs[1].k, "tool.args") == 0);
   assert(strcmp(s.attrs[1].v, "ls -la") == 0);
   assert(strcmp(s.attrs[2].k, "turn") == 0);
   assert(strcmp(s.attrs[2].v, "3") == 0);

   printf("  span_attributes: ok\n");
}

static void test_attr_overflow_clamped(void)
{
   otel_init("http://127.0.0.1:4318", "aimee", "session-overflow-test");

   otel_span_t s;
   otel_span_start(&s, "overflow", NULL);

   /* Add more than OTEL_MAX_ATTRS attributes — extras are silently dropped */
   for (int i = 0; i < OTEL_MAX_ATTRS + 5; i++)
   {
      char k[16];
      snprintf(k, sizeof(k), "key%d", i);
      otel_span_attr_s(&s, k, "val");
   }

   assert(s.attr_count == OTEL_MAX_ATTRS);
   printf("  attr_overflow_clamped: ok\n");
}

static void test_span_end_sets_status(void)
{
   otel_init("http://127.0.0.1:4318", "aimee", "session-status-test");

   otel_span_t ok_span;
   otel_span_start(&ok_span, "ok", NULL);
   otel_span_end(&ok_span, 0);
   assert(ok_span.status_code == 1); /* OK */
   assert(ok_span.end_ns >= ok_span.start_ns);

   otel_span_t err_span;
   otel_span_start(&err_span, "err", NULL);
   otel_span_end(&err_span, 1);
   assert(err_span.status_code == 2); /* ERROR */

   printf("  span_end_sets_status: ok\n");
}

static void test_on_trace_request_response(void)
{
   otel_init("http://127.0.0.1:4318", "aimee", "session-trace-hook");

   /* Simulate a request/response cycle — must not crash */
   otel_on_trace("request", NULL, NULL, NULL, 0);
   otel_on_trace("response", NULL, NULL, NULL, 0);

   printf("  on_trace_request_response: ok\n");
}

static void test_on_trace_tool_call(void)
{
   otel_init("http://127.0.0.1:4318", "aimee", "session-tool-hook");

   /* Emit a turn first so the turn span_id is populated */
   otel_on_trace("request", NULL, NULL, NULL, 1);

   /* Emit a tool call span — must not crash */
   otel_on_trace("tool_call", "bash", "{\"cmd\":\"ls\"}", "file1.c\nfile2.c", 1);

   /* Error tool result detected by "error" prefix */
   otel_on_trace("tool_call", "write_file", "{}", "error: permission denied", 1);

   /* Flush on response */
   otel_on_trace("response", NULL, NULL, NULL, 1);

   printf("  on_trace_tool_call: ok\n");
}

static void test_delegation_start_end(void)
{
   otel_init("http://127.0.0.1:4318", "aimee", "session-deleg-test");

   otel_span_t span;
   otel_delegation_start(&span, "reviewer", "review the auth module");

   assert(strcmp(span.name, "delegate") == 0);
   assert(span.start_ns > 0);

   otel_delegation_end(&span, 0);
   assert(span.status_code == 1);
   assert(span.end_ns >= span.start_ns);

   /* Failed delegation */
   otel_span_t fail_span;
   otel_delegation_start(&fail_span, "fixer", "fix the bug");
   otel_delegation_end(&fail_span, 1);
   assert(fail_span.status_code == 2);

   printf("  delegation_start_end: ok\n");
}

static void test_flush_no_crash_when_disabled(void)
{
   otel_init("", NULL, NULL);
   otel_flush(); /* must be a no-op */
   printf("  flush_no_crash_when_disabled: ok\n");
}

static void test_trace_id_derived_from_session(void)
{
   /* Session ID with dashes is stripped to 32 lowercase hex chars */
   otel_init("http://127.0.0.1:4318", "aimee", "aabbccdd-1122-3344-5566-aabbccddeeff");

   otel_span_t s;
   otel_span_start(&s, "test", NULL);

   /* trace_id should be 32 hex chars */
   assert(strlen(s.trace_id) == 32);
   for (int i = 0; i < 32; i++)
   {
      char c = s.trace_id[i];
      assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
   }

   printf("  trace_id_derived_from_session: ok\n");
}

/* ------------------------------------------------------------------ main */

int main(void)
{
   printf("test_otel:\n");

   test_disabled_by_default();
   test_init_enables_export();
   test_init_empty_disables();
   test_span_start_fills_ids();
   test_span_with_parent();
   test_span_attributes();
   test_attr_overflow_clamped();
   test_span_end_sets_status();
   test_on_trace_request_response();
   test_on_trace_tool_call();
   test_delegation_start_end();
   test_flush_no_crash_when_disabled();
   test_trace_id_derived_from_session();

   printf("All otel tests passed.\n");
   return 0;
}
