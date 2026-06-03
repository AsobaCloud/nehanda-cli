/* server_http_config_routes.inc: /v1 persona and delegate-role-template route
 * handlers, factored out of server_http.c to keep that TU under the line cap.
 * Included once, mid-file, after the JSON/emit helpers it relies on. */
static cJSON *persona_to_json(const persona_t *p)
{
   cJSON *o = cJSON_CreateObject();
   cJSON_AddStringToObject(o, "name", p->name);
   cJSON_AddStringToObject(o, "description", p->description);
   cJSON *roles = cJSON_AddArrayToObject(o, "roles");
   for (int i = 0; i < p->roles_count; i++)
      cJSON_AddItemToArray(roles, cJSON_CreateString(p->roles[i]));
   cJSON_AddStringToObject(o, "check_role", p->check_role);
   cJSON_AddStringToObject(o, "check_marker", p->check_marker);
   cJSON_AddStringToObject(o, "delegates", p->delegates);
   cJSON_AddBoolToObject(o, "builtin", p->builtin);
   /* Full prose so clients can show/round-trip a persona for editing. Built-ins
    * loaded via the code fallback (no file) carry NULL prose; emit "" there. */
   cJSON_AddStringToObject(o, "persona", p->persona_text ? p->persona_text : "");
   cJSON_AddStringToObject(o, "principles", p->principles_text ? p->principles_text : "");
   cJSON_AddStringToObject(o, "brief", p->brief_text ? p->brief_text : "");
   return o;
}

/* Parse a persona create/edit request body into *out (zeroed first). Heap prose
 * fields are owned by the caller (persona_free). `url_name`, when non-empty
 * (from PUT /v1/personas/<name>), takes precedence over a body "name". Returns 0
 * on success; -1 with *errmsg set on a validation error. */
static int persona_from_json(const char *body, const char *url_name, persona_t *out,
                             const char **errmsg)
{
   memset(out, 0, sizeof(*out));
   cJSON *req = body ? cJSON_Parse(body) : NULL;
   if (!req)
   {
      *errmsg = "invalid JSON body";
      return -1;
   }
   const char *name = (url_name && url_name[0]) ? url_name : NULL;
   if (!name)
   {
      cJSON *jn = cJSON_GetObjectItemCaseSensitive(req, "name");
      name = cJSON_IsString(jn) ? jn->valuestring : NULL;
   }
   if (!name || !persona_name_valid(name))
   {
      cJSON_Delete(req);
      *errmsg = "missing or invalid persona name";
      return -1;
   }
   snprintf(out->name, sizeof(out->name), "%s", name);

   cJSON *d = cJSON_GetObjectItemCaseSensitive(req, "description");
   if (cJSON_IsString(d))
      snprintf(out->description, sizeof(out->description), "%s", d->valuestring);
   cJSON *dg = cJSON_GetObjectItemCaseSensitive(req, "delegates");
   if (cJSON_IsString(dg) && dg->valuestring[0])
      snprintf(out->delegates, sizeof(out->delegates), "%s", dg->valuestring);
   else
      snprintf(out->delegates, sizeof(out->delegates), "%s", PERSONA_DELEGATES_FULL);
   cJSON *cr = cJSON_GetObjectItemCaseSensitive(req, "check_role");
   if (cJSON_IsString(cr))
      snprintf(out->check_role, sizeof(out->check_role), "%s", cr->valuestring);
   cJSON *cm = cJSON_GetObjectItemCaseSensitive(req, "check_marker");
   if (cJSON_IsString(cm))
      snprintf(out->check_marker, sizeof(out->check_marker), "%s", cm->valuestring);
   cJSON *roles = cJSON_GetObjectItemCaseSensitive(req, "roles");
   if (cJSON_IsArray(roles))
   {
      cJSON *r = NULL;
      cJSON_ArrayForEach(r, roles)
      {
         if (cJSON_IsString(r) && out->roles_count < PERSONA_MAX_ROLES)
            snprintf(out->roles[out->roles_count++], PERSONA_NAME_MAX, "%s", r->valuestring);
      }
   }
   cJSON *pt = cJSON_GetObjectItemCaseSensitive(req, "persona");
   if (cJSON_IsString(pt) && pt->valuestring[0])
      out->persona_text = safe_strdup(pt->valuestring);
   cJSON *pr = cJSON_GetObjectItemCaseSensitive(req, "principles");
   if (cJSON_IsString(pr) && pr->valuestring[0])
      out->principles_text = safe_strdup(pr->valuestring);
   cJSON *br = cJSON_GetObjectItemCaseSensitive(req, "brief");
   if (cJSON_IsString(br) && br->valuestring[0])
      out->brief_text = safe_strdup(br->valuestring);
   cJSON_Delete(req);
   return 0;
}

/* ── routes ─────────────────────────────────────────────────────────────── */

static int route_personas_list(char *resp, int cap)
{
   char names[PERSONA_MAX_NAMES][PERSONA_NAME_MAX];
   int n = persona_list(NULL, names, PERSONA_MAX_NAMES);
   cJSON *root = cJSON_CreateObject();
   cJSON *arr = cJSON_AddArrayToObject(root, "personas");
   for (int i = 0; i < n; i++)
   {
      persona_t p;
      if (persona_load(NULL, names[i], &p) != 0)
         continue;
      cJSON *item = cJSON_CreateObject();
      cJSON_AddStringToObject(item, "name", p.name);
      cJSON_AddStringToObject(item, "description", p.description);
      cJSON_AddBoolToObject(item, "builtin", p.builtin);
      cJSON_AddItemToArray(arr, item);
      persona_free(&p);
   }
   return emit(resp, cap, root);
}

/* The active durable-default persona (resolved server-side from config/env), so
 * non-session clients (e.g. `aimee manuscript check`) never read config files. */
static int route_persona_current(char *resp, int cap)
{
   const char *name = aimee_mode_to_string(config_current_mode());
   persona_t p;
   persona_load(NULL, name, &p);
   int rc = emit(resp, cap, persona_to_json(&p));
   persona_free(&p);
   return rc;
}

static int route_persona_show(const char *name, char *resp, int cap)
{
   if (!name || !name[0])
      return err_json(resp, cap, 400, "missing persona name");
   if (!persona_is_builtin(name))
   {
      char path[PERSONA_PATH_MAX];
      if (persona_path(NULL, name, path, sizeof(path)) != 0)
         return err_json(resp, cap, 404, "no such persona");
   }
   persona_t p;
   if (persona_load(NULL, name, &p) != 0)
      return err_json(resp, cap, 500, "load failed");
   int rc = emit(resp, cap, persona_to_json(&p));
   persona_free(&p);
   return rc;
}

/* Create or edit a persona: write <config>/personas/<name>.md from the request
 * body, then return the reloaded persona (config is the source of truth). Used
 * by POST /v1/personas (name in body) and PUT /v1/personas/<name>. Editing a
 * built-in name writes an override file the built-in default falls back to when
 * removed. */
static int route_persona_upsert(const char *url_name, const char *body, char *resp, int cap)
{
   persona_t p;
   const char *errmsg = NULL;
   if (persona_from_json(body, url_name, &p, &errmsg) != 0)
      return err_json(resp, cap, 400, errmsg ? errmsg : "bad request");
   int wrote = persona_write(&p);
   persona_free(&p);
   if (wrote != 0)
      return err_json(resp, cap, 500, "failed to write persona");
   persona_t loaded;
   persona_load(NULL, p.name, &loaded);
   int rc = emit(resp, cap, persona_to_json(&loaded));
   persona_free(&loaded);
   return rc;
}

/* Remove a user-level persona file (DELETE /v1/personas/<name>). For a built-in
 * this resets it to the code default; for a custom persona it deletes it. */
static int route_persona_remove(const char *name, char *resp, int cap)
{
   if (!name || !persona_name_valid(name))
      return err_json(resp, cap, 400, "invalid persona name");
   if (persona_delete(name) != 0)
      return err_json(resp, cap, 404, "no user persona file to remove");
   cJSON *o = cJSON_CreateObject();
   cJSON_AddStringToObject(o, "name", name);
   cJSON_AddBoolToObject(o, "deleted", 1);
   return emit(resp, cap, o);
}

/* ── delegate role templates (the delegate analog of personas) ───────────── */

static int route_role_templates_list(char *resp, int cap)
{
   char names[ROLE_TEMPLATE_MAX_ROLES][ROLE_TEMPLATE_NAME_MAX];
   int n = role_template_list(NULL, names, ROLE_TEMPLATE_MAX_ROLES);
   cJSON *root = cJSON_CreateObject();
   cJSON *arr = cJSON_AddArrayToObject(root, "role_templates");
   for (int i = 0; i < n; i++)
      cJSON_AddItemToArray(arr, cJSON_CreateString(names[i]));
   return emit(resp, cap, root);
}

static int route_role_template_show(const char *name, char *resp, int cap)
{
   if (!name || !name[0])
      return err_json(resp, cap, 400, "missing role name");
   char *raw = role_template_read_raw(NULL, name);
   if (!raw)
      return err_json(resp, cap, 404, "no such role template");
   cJSON *o = cJSON_CreateObject();
   cJSON_AddStringToObject(o, "role", name);
   cJSON_AddStringToObject(o, "content", raw);
   free(raw);
   return emit(resp, cap, o);
}

static int route_role_template_upsert(const char *name, const char *body, char *resp, int cap)
{
   if (!role_template_name_valid(name))
      return err_json(resp, cap, 400, "invalid role name");
   cJSON *req = body ? cJSON_Parse(body) : NULL;
   cJSON *jc = req ? cJSON_GetObjectItemCaseSensitive(req, "content") : NULL;
   if (!cJSON_IsString(jc))
   {
      cJSON_Delete(req);
      return err_json(resp, cap, 400, "missing 'content' string");
   }
   int rc = role_template_write(name, jc->valuestring);
   cJSON_Delete(req);
   if (rc != 0)
      return err_json(resp, cap, 500, "failed to write role template");
   char *raw = role_template_read_raw(NULL, name);
   cJSON *o = cJSON_CreateObject();
   cJSON_AddStringToObject(o, "role", name);
   cJSON_AddStringToObject(o, "content", raw ? raw : "");
   free(raw);
   return emit(resp, cap, o);
}

static int route_role_template_remove(const char *name, char *resp, int cap)
{
   if (!role_template_name_valid(name))
      return err_json(resp, cap, 400, "invalid role name");
   if (role_template_delete(name) != 0)
      return err_json(resp, cap, 404, "no user role template to remove");
   cJSON *o = cJSON_CreateObject();
   cJSON_AddStringToObject(o, "role", name);
   cJSON_AddBoolToObject(o, "deleted", 1);
   return emit(resp, cap, o);
}
