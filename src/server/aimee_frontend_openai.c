/* aimee_frontend_openai.c -- OpenAI Chat Completions (/v1/chat/completions) <-> IR.
 * Parse only in this slice. See aimee_frontend.h.
 *
 * NOTE (Slice 1): in-message tool_calls (assistant) + role:"tool" results are
 * handled per the Q1 roundtable ruling on tool-argument representation and are
 * added in the follow-up; this slice covers system-lifting + text/image content +
 * tool DEFINITIONS + sampling params, which is what the basic golden pair needs. */
#include "aimee_frontend.h"

#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) { return s ? strdup(s) : NULL; }

static const char *ostr(const cJSON *o, const char *k)
{
   const cJSON *it = cJSON_GetObjectItemCaseSensitive((cJSON *)o, k);
   return (it && cJSON_IsString(it)) ? it->valuestring : NULL;
}

/* grow `*arr` (of element size `esz`) by one, return the new (zeroed) slot or NULL. */
static void *grow1(void **arr, int *n, size_t esz)
{
   void *p = realloc(*arr, (size_t)(*n + 1) * esz);
   if (!p)
      return NULL;
   *arr = p;
   void *slot = (char *)p + (size_t)(*n) * esz;
   memset(slot, 0, esz);
   (*n)++;
   return slot;
}

/* Parse an OpenAI content value (string OR array of parts) and APPEND blocks to
 * (*blocks,*n). type:"text" -> TEXT; type:"image_url" -> IMAGE (media_ref=url). */
static int append_openai_content(const cJSON *content, aimee_block_t **blocks, int *n)
{
   if (!content)
      return 0;
   if (cJSON_IsString(content))
   {
      aimee_block_t *b = grow1((void **)blocks, n, sizeof(aimee_block_t));
      if (!b)
         return -1;
      b->type = AIMEE_BLK_TEXT;
      b->text = dupstr(content->valuestring);
      return 0;
   }
   if (!cJSON_IsArray(content))
      return 0;
   const cJSON *el = NULL;
   cJSON_ArrayForEach(el, content)
   {
      aimee_block_t *b = grow1((void **)blocks, n, sizeof(aimee_block_t));
      if (!b)
         return -1;
      b->raw = cJSON_Duplicate(el, 1);
      const char *type = ostr(el, "type");
      if (type && strcmp(type, "image_url") == 0)
      {
         b->type = AIMEE_BLK_IMAGE;
         const cJSON *iu = cJSON_GetObjectItemCaseSensitive((cJSON *)el, "image_url");
         b->media_ref = dupstr(iu ? ostr(iu, "url") : NULL);
      }
      else /* text (default) */
      {
         b->type = AIMEE_BLK_TEXT;
         b->text = dupstr(ostr(el, "text"));
      }
   }
   return 0;
}

int openai_frontend_parse(const cJSON *req, aimee_request_t *out, char *err, size_t errn)
{
   if (out)
      memset(out, 0, sizeof *out);
   if (!req || !cJSON_IsObject(req) || !out)
   {
      if (err && errn)
         snprintf(err, errn, "openai_frontend_parse: null/non-object request");
      return -1;
   }
   out->frontend = AIMEE_WIRE_OPENAI_CHAT;
   out->raw = cJSON_Duplicate(req, 1);
   out->model = dupstr(ostr(req, "model"));

   const cJSON *mt = cJSON_GetObjectItemCaseSensitive((cJSON *)req, "max_tokens");
   if (mt && cJSON_IsNumber(mt))
   {
      out->max_tokens = mt->valueint;
      out->has_max_tokens = 1;
   }
   const cJSON *temp = cJSON_GetObjectItemCaseSensitive((cJSON *)req, "temperature");
   if (temp && cJSON_IsNumber(temp))
   {
      out->temperature = temp->valuedouble;
      out->has_temperature = 1;
   }
   const cJSON *stream = cJSON_GetObjectItemCaseSensitive((cJSON *)req, "stream");
   out->stream = (stream && cJSON_IsTrue(stream)) ? 1 : 0;

   /* messages: LIFT role:"system" into out->system (to converge with Anthropic's
    * separate system); everything else becomes an IR message. */
   const cJSON *msgs = cJSON_GetObjectItemCaseSensitive((cJSON *)req, "messages");
   if (msgs && cJSON_IsArray(msgs))
   {
      const cJSON *m = NULL;
      cJSON_ArrayForEach(m, msgs)
      {
         const char *role = ostr(m, "role");
         const cJSON *content = cJSON_GetObjectItemCaseSensitive((cJSON *)m, "content");
         if (role && strcmp(role, "system") == 0)
         {
            if (append_openai_content(content, &out->system, &out->n_system) != 0)
               goto oom;
            continue;
         }
         aimee_message_t *msg = grow1((void **)&out->messages, &out->n_messages,
                                      sizeof(aimee_message_t));
         if (!msg)
            goto oom;
         msg->role = dupstr(role);
         msg->raw = cJSON_Duplicate(m, 1);
         if (append_openai_content(content, &msg->blocks, &msg->n_blocks) != 0)
            goto oom;
      }
   }

   /* tools: {type:function, function:{name,description,parameters}} */
   const cJSON *tools = cJSON_GetObjectItemCaseSensitive((cJSON *)req, "tools");
   if (tools && cJSON_IsArray(tools))
   {
      const cJSON *t = NULL;
      cJSON_ArrayForEach(t, tools)
      {
         const cJSON *fn = cJSON_GetObjectItemCaseSensitive((cJSON *)t, "function");
         const cJSON *src = fn ? fn : t; /* tolerate a flat tool too */
         aimee_tool_t *tool = grow1((void **)&out->tools, &out->n_tools, sizeof(aimee_tool_t));
         if (!tool)
            goto oom;
         tool->name = dupstr(ostr(src, "name"));
         tool->description = dupstr(ostr(src, "description"));
         const cJSON *params = cJSON_GetObjectItemCaseSensitive((cJSON *)src, "parameters");
         tool->schema = params ? cJSON_Duplicate(params, 1) : NULL;
      }
   }

   const cJSON *tc = cJSON_GetObjectItemCaseSensitive((cJSON *)req, "tool_choice");
   out->tool_choice = tc ? cJSON_Duplicate(tc, 1) : NULL;

   /* stop: string OR array */
   const cJSON *stop = cJSON_GetObjectItemCaseSensitive((cJSON *)req, "stop");
   if (stop && cJSON_IsString(stop))
   {
      out->stop_sequences = calloc(1, sizeof(char *));
      if (!out->stop_sequences)
         goto oom;
      out->stop_sequences[0] = dupstr(stop->valuestring);
      out->n_stop = 1;
   }
   else if (stop && cJSON_IsArray(stop))
   {
      const cJSON *s = NULL;
      cJSON_ArrayForEach(s, stop)
      {
         if (!cJSON_IsString(s))
            continue;
         char **slot = grow1((void **)&out->stop_sequences, &out->n_stop, sizeof(char *));
         if (!slot)
            goto oom;
         *slot = dupstr(s->valuestring);
      }
   }
   return 0;

oom:
   if (err && errn)
      snprintf(err, errn, "openai_frontend_parse: out of memory");
   aimee_request_free(out);
   return -1;
}
