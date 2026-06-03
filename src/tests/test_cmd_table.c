/* test_cmd_table.c: command-table tier filtering and visibility tests.
 *
 * Self-contained: uses a local minimal table so we don't need to link LIB_CMD.
 * Tests the tier enum values and the visibility logic mirrored from cmd_table.c.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "aimee.h"

/* Mirror of command_is_alias / command_is_hidden_default from cmd_table.c */
static int local_is_alias(const char *name)
{
   return strcmp(name, "+") == 0 || strcmp(name, "-") == 0 || strcmp(name, "quickstart") == 0 ||
          strcmp(name, "queue") == 0;
}

static int local_is_hidden(const char *name)
{
   return local_is_alias(name) || strcmp(name, "hooks") == 0 ||
          strcmp(name, "session-start") == 0 || strcmp(name, "launch") == 0 ||
          strcmp(name, "wrapup") == 0 || strcmp(name, "help") == 0;
}

/* Minimal tier table: name -> tier, no handler pointers needed */
typedef struct
{
   const char *name;
   cmd_tier_t tier;
} tier_entry_t;

static const tier_entry_t tier_table[] = {
    /* CMD_TIER_CORE */
    {"init", CMD_TIER_CORE},
    {"setup", CMD_TIER_CORE},
    {"quickstart", CMD_TIER_CORE}, /* alias - hidden */
    {"wm", CMD_TIER_CORE},
    {"index", CMD_TIER_CORE},
    {"memory", CMD_TIER_CORE},
    {"rules", CMD_TIER_CORE},
    {"learning", CMD_TIER_CORE},
    {"diagnose", CMD_TIER_CORE},
    {"feedback", CMD_TIER_CORE},
    {"+", CMD_TIER_CORE},             /* alias - hidden */
    {"-", CMD_TIER_CORE},             /* alias - hidden */
    {"hooks", CMD_TIER_CORE},         /* hidden */
    {"session-start", CMD_TIER_CORE}, /* hidden */
    {"launch", CMD_TIER_CORE},        /* hidden */
    {"wrapup", CMD_TIER_CORE},        /* hidden */
    {"mode", CMD_TIER_CORE},
    {"tdd", CMD_TIER_CORE},
    {"plan", CMD_TIER_CORE},
    {"implement", CMD_TIER_CORE},
    {"run", CMD_TIER_CORE},
    {"delegate", CMD_TIER_CORE},
    {"verify", CMD_TIER_CORE},
    {"config", CMD_TIER_CORE},
    {"version", CMD_TIER_CORE},
    {"help", CMD_TIER_CORE}, /* hidden */
    {"notify", CMD_TIER_CORE},
    {"roles", CMD_TIER_CORE},
    {"toolset", CMD_TIER_CORE},
    {"skill", CMD_TIER_CORE},
    /* CMD_TIER_ADVANCED */
    {"workspace", CMD_TIER_ADVANCED},
    {"agent", CMD_TIER_ADVANCED},
    {"context", CMD_TIER_ADVANCED},
    {"dispatch", CMD_TIER_ADVANCED},
    {"queue", CMD_TIER_ADVANCED}, /* alias - hidden */
    {"work", CMD_TIER_ADVANCED},
    {"cancel", CMD_TIER_ADVANCED},
    {"rewind", CMD_TIER_ADVANCED},
    {"trace", CMD_TIER_ADVANCED},
    {"jobs", CMD_TIER_ADVANCED},
    {"job", CMD_TIER_ADVANCED},
    {"autopilot", CMD_TIER_ADVANCED},
    {"plans", CMD_TIER_ADVANCED},
    {"session", CMD_TIER_ADVANCED},
    {"worktree", CMD_TIER_ADVANCED},
    {"status", CMD_TIER_ADVANCED},
    {"hud", CMD_TIER_ADVANCED},
    {"usage", CMD_TIER_ADVANCED},
    {"manifest", CMD_TIER_ADVANCED},
    {"contract", CMD_TIER_ADVANCED},
    {"describe", CMD_TIER_ADVANCED},
    {"env", CMD_TIER_ADVANCED},
    {"doctor", CMD_TIER_ADVANCED},
    {"provider", CMD_TIER_ADVANCED},
    {"aux", CMD_TIER_ADVANCED},
    {"slop", CMD_TIER_ADVANCED},
    /* CMD_TIER_ADMIN */
    {"eval", CMD_TIER_ADMIN},
    {"export", CMD_TIER_ADMIN},
    {"import", CMD_TIER_ADMIN},
    {"db", CMD_TIER_ADMIN},
    {"branch", CMD_TIER_ADMIN},
    {"git", CMD_TIER_ADMIN},
    {"clean", CMD_TIER_ADMIN},
    {NULL, 0}};

static int count_visible(cmd_tier_t tier)
{
   int n = 0;
   for (int i = 0; tier_table[i].name != NULL; i++)
      if (tier_table[i].tier == tier && !local_is_hidden(tier_table[i].name))
         n++;
   return n;
}

static void test_every_tier_value_is_valid(void)
{
   for (int i = 0; tier_table[i].name != NULL; i++)
   {
      cmd_tier_t t = tier_table[i].tier;
      assert(t == CMD_TIER_CORE || t == CMD_TIER_ADVANCED || t == CMD_TIER_ADMIN);
   }
}

static void test_each_tier_has_visible_commands(void)
{
   assert(count_visible(CMD_TIER_CORE) > 0);
   assert(count_visible(CMD_TIER_ADVANCED) > 0);
   assert(count_visible(CMD_TIER_ADMIN) > 0);
}

static void test_admin_is_smallest_visible_tier(void)
{
   /* Admin (power-user only) must be the smallest tier by visible count. */
   assert(count_visible(CMD_TIER_ADMIN) <= count_visible(CMD_TIER_CORE));
   assert(count_visible(CMD_TIER_ADMIN) <= count_visible(CMD_TIER_ADVANCED));
}

static void test_known_core_commands(void)
{
   const char *must_be_core[] = {"init",   "memory", "rules", "learning", "delegate",
                                 "config", "verify", "skill", "notify",   NULL};
   for (int k = 0; must_be_core[k]; k++)
   {
      int found = 0;
      for (int i = 0; tier_table[i].name != NULL; i++)
         if (strcmp(tier_table[i].name, must_be_core[k]) == 0)
         {
            assert(tier_table[i].tier == CMD_TIER_CORE);
            found = 1;
            break;
         }
      assert(found);
   }
}

static void test_known_advanced_commands(void)
{
   const char *must_be_adv[] = {"work",      "worktree", "session", "status", "trace",
                                "autopilot", "doctor",   "hud",     NULL};
   for (int k = 0; must_be_adv[k]; k++)
   {
      int found = 0;
      for (int i = 0; tier_table[i].name != NULL; i++)
         if (strcmp(tier_table[i].name, must_be_adv[k]) == 0)
         {
            assert(tier_table[i].tier == CMD_TIER_ADVANCED);
            found = 1;
            break;
         }
      assert(found);
   }
}

static void test_known_admin_commands(void)
{
   const char *must_be_admin[] = {"eval", "db", "branch", "git", "export", "import", "clean", NULL};
   for (int k = 0; must_be_admin[k]; k++)
   {
      int found = 0;
      for (int i = 0; tier_table[i].name != NULL; i++)
         if (strcmp(tier_table[i].name, must_be_admin[k]) == 0)
         {
            assert(tier_table[i].tier == CMD_TIER_ADMIN);
            found = 1;
            break;
         }
      assert(found);
   }
}

static void test_aliases_are_hidden(void)
{
   assert(local_is_alias("+"));
   assert(local_is_alias("-"));
   assert(local_is_alias("quickstart"));
   assert(local_is_alias("queue"));
   assert(!local_is_alias("init"));
   assert(!local_is_alias("work"));
   assert(!local_is_alias("help"));
   assert(!local_is_alias(""));
}

static void test_lifecycle_commands_are_hidden(void)
{
   const char *lifecycle[] = {"hooks", "session-start", "launch", "wrapup", "help", NULL};
   for (int k = 0; lifecycle[k]; k++)
      assert(local_is_hidden(lifecycle[k]));
}

static void test_visible_commands_are_not_hidden(void)
{
   const char *must_show[] = {"init", "memory", "config", "verify",   "work", "status",
                              "eval", "db",     "git",    "delegate", NULL};
   for (int k = 0; must_show[k]; k++)
      assert(!local_is_hidden(must_show[k]));
}

static void test_hidden_commands_excluded_from_default_view(void)
{
   const char *hidden[] = {"+",      "-",      "quickstart", "queue", "hooks", "session-start",
                           "launch", "wrapup", "help",       NULL};
   for (int k = 0; hidden[k]; k++)
      assert(local_is_hidden(hidden[k]));
}

int main(void)
{
   printf("cmd_table: ");

   test_every_tier_value_is_valid();
   test_each_tier_has_visible_commands();
   test_admin_is_smallest_visible_tier();
   test_known_core_commands();
   test_known_advanced_commands();
   test_known_admin_commands();
   test_aliases_are_hidden();
   test_lifecycle_commands_are_hidden();
   test_visible_commands_are_not_hidden();
   test_hidden_commands_excluded_from_default_view();

   printf("all tests passed\n");
   return 0;
}
