/* test_plugin.c: unit tests for the plugin system */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "aimee.h"
#include "../headers/plugin.h"
#include "cJSON.h"
#include "platform_test_util.h"

/* Write a minimal plugin.json manifest into a temp dir */
static void write_manifest(const char *plugin_dir, const char *json)
{
   char manifest_dir[512];
   snprintf(manifest_dir, sizeof(manifest_dir), "%s/.aimee-plugin", plugin_dir);
   mkdir(manifest_dir, 0755);

   char manifest_path[512];
   snprintf(manifest_path, sizeof(manifest_path), "%s/plugin.json", manifest_dir);
   FILE *f = fopen(manifest_path, "w");
   assert(f != NULL);
   fputs(json, f);
   fclose(f);
}

int main(void)
{
   printf("plugin: ");

   /* Use isolated HOME so registry writes don't pollute the real one */
   char tmpdir[512];
   snprintf(tmpdir, sizeof(tmpdir), "%s/aimee-test-plugin-XXXXXX", platform_tmpdir());
   assert(platform_mkdtemp(tmpdir) != NULL);
   platform_setenv("HOME", tmpdir);

   /* Force plugin_registry_path to re-compute against new HOME */
   /* plugin_registry_path caches the path — set env before first call */

   /* ---------------------------------------------------------------
    * 1. Permission helpers
    * ------------------------------------------------------------- */
   {
      assert(strcmp(plugin_permission_name(PLUGIN_PERM_READ), "read") == 0);
      assert(strcmp(plugin_permission_name(PLUGIN_PERM_WRITE), "write") == 0);
      assert(strcmp(plugin_permission_name(PLUGIN_PERM_EXECUTE), "execute") == 0);
      assert(strcmp(plugin_permission_name(PLUGIN_PERM_DANGEROUS), "dangerous") == 0);

      assert(plugin_permission_from_str("read") == PLUGIN_PERM_READ);
      assert(plugin_permission_from_str("write") == PLUGIN_PERM_WRITE);
      assert(plugin_permission_from_str("execute") == PLUGIN_PERM_EXECUTE);
      assert(plugin_permission_from_str("dangerous") == PLUGIN_PERM_DANGEROUS);
      assert(plugin_permission_from_str(NULL) == PLUGIN_PERM_READ);
      assert(plugin_permission_from_str("unknown") == PLUGIN_PERM_READ);
   }

   /* ---------------------------------------------------------------
    * 2. Empty registry load
    * ------------------------------------------------------------- */
   {
      plugin_t plugins[8];
      int count = plugin_registry_load(plugins, 8);
      assert(count == 0);
   }

   /* ---------------------------------------------------------------
    * 3. Manifest parsing: minimal manifest
    * ------------------------------------------------------------- */
   {
      char plugin_dir[512];
      snprintf(plugin_dir, sizeof(plugin_dir), "%s/plugin-minimal", tmpdir);
      mkdir(plugin_dir, 0755);
      write_manifest(plugin_dir, "{"
                                 "  \"name\": \"test-minimal\","
                                 "  \"version\": \"1.0.0\","
                                 "  \"description\": \"A minimal test plugin\""
                                 "}");

      plugin_t p;
      char err[256];
      int rc = plugin_manifest_parse(plugin_dir, &p, err, sizeof(err));
      assert(rc == 0);
      assert(strcmp(p.name, "test-minimal") == 0);
      assert(strcmp(p.version, "1.0.0") == 0);
      assert(strcmp(p.description, "A minimal test plugin") == 0);
      assert(strcmp(p.source_path, plugin_dir) == 0);
      assert(p.hook_count == 0);
      assert(p.tool_count == 0);
   }

   /* ---------------------------------------------------------------
    * 4. Manifest parsing: hooks and tools
    * ------------------------------------------------------------- */
   {
      char plugin_dir[512];
      snprintf(plugin_dir, sizeof(plugin_dir), "%s/plugin-full", tmpdir);
      mkdir(plugin_dir, 0755);
      write_manifest(plugin_dir, "{"
                                 "  \"name\": \"test-full\","
                                 "  \"version\": \"0.2.0\","
                                 "  \"description\": \"Plugin with hooks and tools\","
                                 "  \"hooks\": {"
                                 "    \"PreToolUse\":  [\"/hook-pre.sh\"],"
                                 "    \"PostToolUse\": [\"/hook-post.sh\"]"
                                 "  },"
                                 "  \"tools\": ["
                                 "    {"
                                 "      \"name\": \"my_tool\","
                                 "      \"description\": \"Does something\","
                                 "      \"command\": \"/my-tool.sh\","
                                 "      \"permission\": \"execute\","
                                 "      \"input_schema\": {\"type\": \"object\"}"
                                 "    }"
                                 "  ]"
                                 "}");

      plugin_t p;
      char err[256];
      int rc = plugin_manifest_parse(plugin_dir, &p, err, sizeof(err));
      assert(rc == 0);
      assert(strcmp(p.name, "test-full") == 0);
      assert(p.hook_count == 2);
      assert(strcmp(p.hooks[0].event, "PreToolUse") == 0);
      assert(strcmp(p.hooks[1].event, "PostToolUse") == 0);
      assert(p.tool_count == 1);
      assert(strcmp(p.tools[0].name, "my_tool") == 0);
      assert(p.tools[0].permission == PLUGIN_PERM_EXECUTE);
   }

   /* ---------------------------------------------------------------
    * 5. Manifest parsing: missing file returns error
    * ------------------------------------------------------------- */
   {
      plugin_t p;
      char err[256];
      int rc = plugin_manifest_parse("/nonexistent/dir", &p, err, sizeof(err));
      assert(rc == -1);
      assert(err[0] != '\0');
   }

   /* ---------------------------------------------------------------
    * 6. plugin_install: installs, appends to registry
    * ------------------------------------------------------------- */
   {
      char plugin_dir[512];
      snprintf(plugin_dir, sizeof(plugin_dir), "%s/plugin-install", tmpdir);
      mkdir(plugin_dir, 0755);
      write_manifest(plugin_dir, "{"
                                 "  \"name\": \"installable\","
                                 "  \"version\": \"1.2.3\","
                                 "  \"description\": \"To be installed\""
                                 "}");

      char err[256];
      int rc = plugin_install(plugin_dir, err, sizeof(err));
      assert(rc == 0);

      plugin_t plugins[8];
      int count = plugin_registry_load(plugins, 8);
      assert(count == 1);
      assert(strcmp(plugins[0].name, "installable") == 0);
      assert(strcmp(plugins[0].version, "1.2.3") == 0);
   }

   /* ---------------------------------------------------------------
    * 7. plugin_install: duplicate name returns error
    * ------------------------------------------------------------- */
   {
      char plugin_dir[512];
      snprintf(plugin_dir, sizeof(plugin_dir), "%s/plugin-install", tmpdir);
      /* already installed from test 6 */
      char err[256];
      int rc = plugin_install(plugin_dir, err, sizeof(err));
      assert(rc == -1);
      assert(strstr(err, "already") != NULL || err[0] != '\0');
   }

   /* ---------------------------------------------------------------
    * 8. plugin_set_enabled: enable/disable round-trip
    * ------------------------------------------------------------- */
   {
      char err[256];

      /* Disable */
      int rc = plugin_set_enabled("installable", 0, err, sizeof(err));
      assert(rc == 0);
      plugin_t plugins[8];
      int count = plugin_registry_load(plugins, 8);
      assert(count >= 1);
      int found = 0;
      for (int i = 0; i < count; i++)
         if (strcmp(plugins[i].name, "installable") == 0)
         {
            assert(plugins[i].enabled == 0);
            found = 1;
         }
      assert(found);

      /* Re-enable */
      rc = plugin_set_enabled("installable", 1, err, sizeof(err));
      assert(rc == 0);
      count = plugin_registry_load(plugins, 8);
      found = 0;
      for (int i = 0; i < count; i++)
         if (strcmp(plugins[i].name, "installable") == 0)
         {
            assert(plugins[i].enabled == 1);
            found = 1;
         }
      assert(found);
   }

   /* ---------------------------------------------------------------
    * 9. plugin_set_enabled: unknown name returns error
    * ------------------------------------------------------------- */
   {
      char err[256];
      int rc = plugin_set_enabled("no-such-plugin", 1, err, sizeof(err));
      assert(rc == -1);
      assert(err[0] != '\0');
   }

   /* ---------------------------------------------------------------
    * 10. plugin_collect_hooks: returns hooks from enabled plugins
    * ------------------------------------------------------------- */
   {
      char plugin_dir[512];
      snprintf(plugin_dir, sizeof(plugin_dir), "%s/plugin-hooks", tmpdir);
      mkdir(plugin_dir, 0755);
      write_manifest(plugin_dir, "{"
                                 "  \"name\": \"hook-plugin\","
                                 "  \"version\": \"1.0.0\","
                                 "  \"defaultEnabled\": true,"
                                 "  \"hooks\": {"
                                 "    \"PreToolUse\":  [\"/bin/true\"],"
                                 "    \"PostToolUse\": [\"/bin/false\"]"
                                 "  }"
                                 "}");

      char err[256];
      assert(plugin_install(plugin_dir, err, sizeof(err)) == 0);

      plugin_t plugins[8];
      int count = plugin_registry_load(plugins, 8);

      char cmds[16][512];
      int hcount = plugin_collect_hooks(plugins, count, "PreToolUse", cmds, 16);
      assert(hcount >= 1);
      int found = 0;
      for (int i = 0; i < hcount; i++)
         if (strcmp(cmds[i], "/bin/true") == 0)
            found = 1;
      assert(found);

      /* PostToolUse */
      hcount = plugin_collect_hooks(plugins, count, "PostToolUse", cmds, 16);
      assert(hcount >= 1);
      found = 0;
      for (int i = 0; i < hcount; i++)
         if (strcmp(cmds[i], "/bin/false") == 0)
            found = 1;
      assert(found);
   }

   /* ---------------------------------------------------------------
    * 11. plugin_collect_tools: returns tools from enabled plugins
    * ------------------------------------------------------------- */
   {
      char plugin_dir[512];
      snprintf(plugin_dir, sizeof(plugin_dir), "%s/plugin-tools", tmpdir);
      mkdir(plugin_dir, 0755);
      write_manifest(plugin_dir, "{"
                                 "  \"name\": \"tool-plugin\","
                                 "  \"version\": \"1.0.0\","
                                 "  \"defaultEnabled\": true,"
                                 "  \"tools\": ["
                                 "    {"
                                 "      \"name\": \"greet\","
                                 "      \"description\": \"Says hello\","
                                 "      \"command\": \"/bin/echo\","
                                 "      \"permission\": \"read\""
                                 "    }"
                                 "  ]"
                                 "}");

      char err[256];
      assert(plugin_install(plugin_dir, err, sizeof(err)) == 0);

      plugin_t plugins[8];
      int count = plugin_registry_load(plugins, 8);

      plugin_tool_t tools[32];
      int tcount = plugin_collect_tools(plugins, count, tools, 32);
      assert(tcount >= 1);
      int found = 0;
      for (int i = 0; i < tcount; i++)
         if (strcmp(tools[i].name, "greet") == 0)
         {
            assert(tools[i].permission == PLUGIN_PERM_READ);
            found = 1;
         }
      assert(found);
   }

   /* ---------------------------------------------------------------
    * 12. plugin_collect_hooks: disabled plugin contributes no hooks
    * ------------------------------------------------------------- */
   {
      char err[256];
      assert(plugin_set_enabled("hook-plugin", 0, err, sizeof(err)) == 0);

      plugin_t plugins[8];
      int count = plugin_registry_load(plugins, 8);

      char cmds[16][512];
      int hcount = plugin_collect_hooks(plugins, count, "PreToolUse", cmds, 16);
      /* hook-plugin is disabled, so /bin/true should not appear */
      for (int i = 0; i < hcount; i++)
         assert(strcmp(cmds[i], "/bin/true") != 0);

      /* Re-enable for cleanliness */
      assert(plugin_set_enabled("hook-plugin", 1, err, sizeof(err)) == 0);
   }

   /* ---------------------------------------------------------------
    * 13. plugin_tool_conflicts_with_builtin: known builtins conflict
    * ------------------------------------------------------------- */
   {
      /* get_help is a known builtin */
      assert(plugin_tool_conflicts_with_builtin("get_help") == 1);
      /* unknown name should not conflict */
      assert(plugin_tool_conflicts_with_builtin("my_custom_tool_xyz") == 0);
   }

   /* ---------------------------------------------------------------
    * 14. plugin_remove: removes from registry
    * ------------------------------------------------------------- */
   {
      char err[256];
      int rc = plugin_remove("installable", err, sizeof(err));
      assert(rc == 0);

      plugin_t plugins[8];
      int count = plugin_registry_load(plugins, 8);
      for (int i = 0; i < count; i++)
         assert(strcmp(plugins[i].name, "installable") != 0);
   }

   /* ---------------------------------------------------------------
    * 15. plugin_remove: unknown name returns error
    * ------------------------------------------------------------- */
   {
      char err[256];
      int rc = plugin_remove("not-there", err, sizeof(err));
      assert(rc == -1);
      assert(err[0] != '\0');
   }

   /* ---------------------------------------------------------------
    * 16. plugin_registry_get / plugin_registry_json helpers
    * ------------------------------------------------------------- */
   {
      plugin_t plugin;
      assert(plugin_registry_get("hook-plugin", &plugin) == 0);
      assert(strcmp(plugin.name, "hook-plugin") == 0);
      assert(plugin.enabled == 1);
      assert(plugin_registry_get("missing-plugin", &plugin) == -1);

      char *json = plugin_registry_json();
      assert(json != NULL);
      cJSON *arr = cJSON_Parse(json);
      assert(cJSON_IsArray(arr));
      int found = 0;
      cJSON *item = NULL;
      cJSON_ArrayForEach(item, arr)
      {
         cJSON *name = cJSON_GetObjectItemCaseSensitive(item, "name");
         if (!cJSON_IsString(name) || strcmp(name->valuestring, "hook-plugin") != 0)
            continue;
         cJSON *enabled = cJSON_GetObjectItemCaseSensitive(item, "enabled");
         cJSON *hooks = cJSON_GetObjectItemCaseSensitive(item, "hook_count");
         cJSON *tools = cJSON_GetObjectItemCaseSensitive(item, "tool_count");
         assert(cJSON_IsBool(enabled));
         assert(cJSON_IsNumber(hooks) && hooks->valueint == 2);
         assert(cJSON_IsNumber(tools) && tools->valueint == 0);
         found = 1;
      }
      assert(found);
      cJSON_Delete(arr);
      free(json);
   }

   /* Cleanup */
   char cmd[512];
   snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
   (void)system(cmd);

   printf("all tests passed\n");
   return 0;
}
