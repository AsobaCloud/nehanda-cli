/* test_delivery_target.c: delivery target parser tests. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "delivery_target.h"

#define PASS(name) printf("  PASS: %s\n", name)

static void assert_target(const char *spec, const char *platform, const char *chat,
                          const char *thread)
{
   delivery_target_t t;
   char roundtrip[256];
   assert(delivery_target_parse(spec, &t) == 0);
   assert(strcmp(t.platform, platform) == 0);
   assert(strcmp(t.chat_id, chat) == 0);
   assert(strcmp(t.thread_id, thread) == 0);
   assert(delivery_target_format(&t, roundtrip, sizeof(roundtrip)) == 0);
   assert(strcmp(roundtrip, spec) == 0);
}

static void test_parse_platform_only_targets(void)
{
   assert_target("telegram", "telegram", "", "");
   assert_target("local", "local", "", "");
   assert_target("origin", "origin", "", "");

   delivery_target_t t;
   assert(delivery_target_parse("origin", &t) == 0);
   assert(delivery_target_is_origin(&t) == 1);
   PASS("parse_platform_only_targets");
}

static void test_parse_channel_targets(void)
{
   assert_target("telegram:-1001234567890", "telegram", "-1001234567890", "");
   assert_target("ntfy:homelab-alerts", "ntfy", "homelab-alerts", "");
   assert_target("webhook:slack-incident-room", "webhook", "slack-incident-room", "");
   PASS("parse_channel_targets");
}

static void test_parse_thread_targets(void)
{
   assert_target("telegram:-1001234567890:42", "telegram", "-1001234567890", "42");
   PASS("parse_thread_targets");
}

static void test_rejects_malformed_targets(void)
{
   delivery_target_t t;
   assert(delivery_target_parse(NULL, &t) == -1);
   assert(delivery_target_parse("", &t) == -1);
   assert(delivery_target_parse(":chat", &t) == -1);
   assert(delivery_target_parse("Telegram:chat", &t) == -1);
   assert(delivery_target_parse("telegram:", &t) == -1);
   assert(delivery_target_parse("telegram:chat:", &t) == -1);
   assert(delivery_target_parse("telegram:chat:thread:extra", &t) == -1);
   assert(delivery_target_parse("telegram:bad\nchat", &t) == -1);
   PASS("rejects_malformed_targets");
}

int main(void)
{
   printf("Running delivery_target tests\n");
   test_parse_platform_only_targets();
   test_parse_channel_targets();
   test_parse_thread_targets();
   test_rejects_malformed_targets();
   printf("All delivery_target tests passed.\n");
   return 0;
}
