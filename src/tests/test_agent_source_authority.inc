static void test_source_authority_overlay_tools(void)
{
   char tmpdir[512];
   snprintf(tmpdir, sizeof(tmpdir), "%s/aimee-source-overlay-XXXXXX", platform_tmpdir());
   assert(platform_mkdtemp(tmpdir) != NULL);

   char path[768];
   snprintf(path, sizeof(path), "%s/current_source.c", tmpdir);
   FILE *f = fopen(path, "w");
   assert(f != NULL);
   fputs("int overlay_unique_symbol(void) { return 42; }\n", f);
   fclose(f);

   assert(platform_setenv("AIMEE_DELEGATE_SOURCE_AUTHORITY", "1") == 0);
   assert(platform_setenv("AIMEE_DELEGATE_SOURCE_PATHS", path) == 0);
   assert(platform_setenv("AIMEE_DELEGATE_WORKTREE_ROOT", tmpdir) == 0);

   char *result = tool_code_search("overlay_unique_symbol", NULL, 5);
   assert(result != NULL);
   cJSON *arr = parse_json_or_die(result);
   assert(cJSON_IsArray(arr));
   cJSON *first = cJSON_GetArrayItem(arr, 0);
   assert(first != NULL);
   assert(strcmp(cJSON_GetObjectItem(first, "project")->valuestring, "current_overlay") == 0);
   assert(strcmp(cJSON_GetObjectItem(first, "freshness")->valuestring, "source_packet_current") ==
          0);
   assert(strcmp(cJSON_GetObjectItem(first, "authority")->valuestring, "current_source") == 0);
   cJSON_Delete(arr);
   free(result);

   result = tool_find_symbol("overlay_unique_symbol");
   assert(result != NULL);
   assert(strstr(result, "source_packet_current") != NULL);
   assert(strstr(result, "authority=current_source") != NULL);
   free(result);

   assert(platform_setenv("AIMEE_DELEGATE_SOURCE_AUTHORITY", "") == 0);
   assert(platform_setenv("AIMEE_DELEGATE_SOURCE_PATHS", "") == 0);
   assert(platform_setenv("AIMEE_DELEGATE_WORKTREE_ROOT", "") == 0);
   unlink(path);
   rmdir(tmpdir);
}
