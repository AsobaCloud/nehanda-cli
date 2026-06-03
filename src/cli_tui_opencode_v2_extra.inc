static cJSON *opencode_v2_legacy_capabilities_json(void)
{
   cJSON *cap = cJSON_CreateObject();
   cJSON_AddBoolToObject(cap, "temperature", 1);
   cJSON_AddBoolToObject(cap, "reasoning", 0);
   cJSON_AddBoolToObject(cap, "attachment", 1);
   cJSON_AddBoolToObject(cap, "toolcall", 1);
   cJSON *input = cJSON_AddObjectToObject(cap, "input");
   cJSON_AddBoolToObject(input, "text", 1);
   cJSON_AddBoolToObject(input, "audio", 0);
   cJSON_AddBoolToObject(input, "image", 1);
   cJSON_AddBoolToObject(input, "video", 0);
   cJSON_AddBoolToObject(input, "pdf", 1);
   cJSON *output = cJSON_AddObjectToObject(cap, "output");
   cJSON_AddBoolToObject(output, "text", 1);
   cJSON_AddBoolToObject(output, "audio", 0);
   cJSON_AddBoolToObject(output, "image", 0);
   cJSON_AddBoolToObject(output, "video", 0);
   cJSON_AddBoolToObject(output, "pdf", 0);
   cJSON_AddBoolToObject(cap, "interleaved", 0);
   return cap;
}

static cJSON *opencode_v2_model_legacy_json(opencode_v2_bridge_t *b)
{
   cJSON *m = cJSON_CreateObject();
   const char *pid = opencode_v2_provider_id(b);
   const char *mid = opencode_v2_model_id(b);
   cJSON_AddStringToObject(m, "id", mid);
   cJSON_AddStringToObject(m, "providerID", pid);
   cJSON *api = cJSON_AddObjectToObject(m, "api");
   cJSON_AddStringToObject(api, "id", mid);
   cJSON_AddStringToObject(api, "url", "");
   cJSON_AddStringToObject(api, "npm", "");
   cJSON_AddStringToObject(m, "name", opencode_v2_model_name(b));
   cJSON_AddStringToObject(m, "family", "aimee");
   cJSON_AddItemToObject(m, "capabilities", opencode_v2_legacy_capabilities_json());
   cJSON *cost = cJSON_AddObjectToObject(m, "cost");
   cJSON_AddNumberToObject(cost, "input", 0);
   cJSON_AddNumberToObject(cost, "output", 0);
   cJSON *cache = cJSON_AddObjectToObject(cost, "cache");
   cJSON_AddNumberToObject(cache, "read", 0);
   cJSON_AddNumberToObject(cache, "write", 0);
   cJSON *limit = cJSON_AddObjectToObject(m, "limit");
   cJSON_AddNumberToObject(limit, "context", 200000);
   cJSON_AddNumberToObject(limit, "input", 200000);
   cJSON_AddNumberToObject(limit, "output", 32000);
   cJSON_AddStringToObject(m, "status", "active");
   cJSON_AddItemToObject(m, "options", cJSON_CreateObject());
   cJSON_AddItemToObject(m, "headers", cJSON_CreateObject());
   cJSON_AddStringToObject(m, "release_date", "");
   cJSON_AddItemToObject(m, "variants", cJSON_CreateObject());
   return m;
}

static cJSON *opencode_v2_provider_legacy_json(opencode_v2_bridge_t *b)
{
   cJSON *p = cJSON_CreateObject();
   const char *pid = opencode_v2_provider_id(b);
   cJSON_AddStringToObject(p, "id", pid);
   cJSON_AddStringToObject(p, "name", "");
   cJSON_AddStringToObject(p, "source", "custom");
   cJSON_AddItemToObject(p, "env", cJSON_CreateArray());
   cJSON_AddItemToObject(p, "options", cJSON_CreateObject());
   cJSON *models = cJSON_AddObjectToObject(p, "models");
   cJSON_AddItemToObject(models, opencode_v2_model_id(b), opencode_v2_model_legacy_json(b));
   return p;
}

static cJSON *opencode_v2_provider_list_legacy_json(opencode_v2_bridge_t *b)
{
   cJSON *root = cJSON_CreateObject();
   cJSON *all = cJSON_AddArrayToObject(root, "all");
   cJSON_AddItemToArray(all, opencode_v2_provider_legacy_json(b));
   cJSON *def = cJSON_AddObjectToObject(root, "default");
   cJSON_AddStringToObject(def, "providerID", opencode_v2_provider_id(b));
   cJSON_AddStringToObject(def, "modelID", opencode_v2_model_id(b));
   cJSON *connected = cJSON_AddArrayToObject(root, "connected");
   cJSON_AddItemToArray(connected, cJSON_CreateString(opencode_v2_provider_id(b)));
   return root;
}

static cJSON *opencode_v2_config_providers_json(opencode_v2_bridge_t *b)
{
   cJSON *root = cJSON_CreateObject();
   cJSON *providers = cJSON_AddArrayToObject(root, "providers");
   cJSON_AddItemToArray(providers, opencode_v2_provider_legacy_json(b));
   cJSON *def = cJSON_AddObjectToObject(root, "default");
   cJSON_AddStringToObject(def, opencode_v2_provider_id(b), opencode_v2_model_id(b));
   return root;
}

static int opencode_v2_resolve_file_path(opencode_v2_bridge_t *b, const char *query, char *out,
                                         size_t out_len)
{
   char *param = opencode_v2_query_param(query, "path");
   const char *rel = param && param[0] ? param : ".";
   int ok = rel[0] == '/' ? snprintf(out, out_len, "%s", rel) > 0
                          : snprintf(out, out_len, "%s/%s", b->root, rel) > 0;
   free(param);
   return ok ? 0 : -1;
}

static cJSON *opencode_v2_file_node_json(const char *base_path, const char *name,
                                         const char *abs_path)
{
   struct stat st;
   cJSON *node = cJSON_CreateObject();
   cJSON_AddStringToObject(node, "name", name);
   if (base_path && base_path[0] && strcmp(base_path, ".") != 0)
   {
      char rel[CLI_TUI_PATH_MAX];
      snprintf(rel, sizeof(rel), "%s/%s", base_path, name);
      cJSON_AddStringToObject(node, "path", rel);
   }
   else
      cJSON_AddStringToObject(node, "path", name);
   cJSON_AddStringToObject(node, "absolute", abs_path);
   cJSON_AddStringToObject(node, "type",
                           stat(abs_path, &st) == 0 && S_ISDIR(st.st_mode) ? "directory" : "file");
   cJSON_AddBoolToObject(node, "ignored", 0);
   return node;
}

static cJSON *opencode_v2_file_list_json_locked(opencode_v2_bridge_t *b, const char *query)
{
   char abs[CLI_TUI_PATH_MAX];
   if (opencode_v2_resolve_file_path(b, query, abs, sizeof(abs)) != 0)
      return cJSON_CreateArray();
   char *base = opencode_v2_query_param(query, "path");
   DIR *d = opendir(abs);
   cJSON *arr = cJSON_CreateArray();
   if (!d)
   {
      free(base);
      return arr;
   }
   struct dirent *ent;
   while ((ent = readdir(d)) != NULL)
   {
      if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
         continue;
      char child[CLI_TUI_PATH_MAX];
      if (snprintf(child, sizeof(child), "%s/%s", abs, ent->d_name) > 0)
         cJSON_AddItemToArray(arr,
                              opencode_v2_file_node_json(base ? base : ".", ent->d_name, child));
   }
   closedir(d);
   free(base);
   return arr;
}

static cJSON *opencode_v2_file_content_json_locked(opencode_v2_bridge_t *b, const char *query)
{
   char abs[CLI_TUI_PATH_MAX];
   cJSON *root = cJSON_CreateObject();
   if (opencode_v2_resolve_file_path(b, query, abs, sizeof(abs)) != 0)
      goto empty;
   FILE *fp = fopen(abs, "rb");
   if (!fp)
      goto empty;
   size_t cap = 1024 * 1024;
   char *buf = malloc(cap + 1);
   size_t n = buf ? fread(buf, 1, cap, fp) : 0;
   fclose(fp);
   if (!buf)
      goto empty;
   int binary = 0;
   for (size_t i = 0; i < n && !binary; i++)
      binary = buf[i] == '\0';
   buf[n] = '\0';
   cJSON_AddStringToObject(root, "type", binary ? "binary" : "text");
   cJSON_AddStringToObject(root, "content", binary ? "" : buf);
   free(buf);
   return root;
empty:
   cJSON_AddStringToObject(root, "type", "text");
   cJSON_AddStringToObject(root, "content", "");
   return root;
}
