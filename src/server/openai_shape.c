/* openai_shape.c: OpenAI-compatible JSON shaping helpers (no sockets, no network). */
#include "openai_shape.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Parse helpers ─────────────────────────────────────────────────────── */

static int parse_model(const cJSON *root, char *model, size_t model_n)
{
   const cJSON *m = cJSON_GetObjectItemCaseSensitive(root, "model");
   if (cJSON_IsString(m) && m->valuestring && m->valuestring[0])
      snprintf(model, model_n, "%s", m->valuestring);
   else
      snprintf(model, model_n, "%s", "aimee");
   return 0;
}

static int parse_stream(const cJSON *root)
{
   const cJSON *s = cJSON_GetObjectItemCaseSensitive(root, "stream");
   return cJSON_IsTrue(s) ? 1 : 0;
}

/* ── openai_parse_chat_request ─────────────────────────────────────────── */

int openai_parse_chat_request(const char *body, char *model, size_t model_n, char **prompt_out,
                              int *stream_out)
{
   if (!body || !prompt_out)
      return -1;
   *prompt_out = NULL;
   if (stream_out)
      *stream_out = 0;

   cJSON *root = cJSON_Parse(body);
   if (!root)
      return -1;

   if (model && model_n)
      parse_model(root, model, model_n);
   if (stream_out)
      *stream_out = parse_stream(root);

   const cJSON *messages = cJSON_GetObjectItemCaseSensitive(root, "messages");
   if (!cJSON_IsArray(messages))
   {
      cJSON_Delete(root);
      return -1;
   }

   /* Dynamically grow a heap buffer for the flattened transcript. */
   size_t buf_cap = 256;
   size_t buf_len = 0;
   char *buf = malloc(buf_cap);
   if (!buf)
   {
      cJSON_Delete(root);
      return -1;
   }
   buf[0] = '\0';

   int found = 0;
   cJSON *msg;
   cJSON_ArrayForEach(msg, messages)
   {
      const cJSON *roleItem = cJSON_GetObjectItemCaseSensitive(msg, "role");
      const char *role = cJSON_IsString(roleItem) ? roleItem->valuestring : "user";

      const cJSON *contentItem = cJSON_GetObjectItemCaseSensitive(msg, "content");
      if (!cJSON_IsString(contentItem) || !contentItem->valuestring || !contentItem->valuestring[0])
         continue;

      const char *content = contentItem->valuestring;
      size_t need = strlen(role) + strlen(content) + 4; /* ": \n\0" */
      while (buf_len + need > buf_cap)
      {
         buf_cap *= 2;
         char *tmp = realloc(buf, buf_cap);
         if (!tmp)
         {
            free(buf);
            cJSON_Delete(root);
            return -1;
         }
         buf = tmp;
      }
      buf_len += snprintf(buf + buf_len, buf_cap - buf_len, "%s: %s\n", role, content);
      found = 1;
   }

   cJSON_Delete(root);

   if (!found)
   {
      free(buf);
      *prompt_out = NULL;
      return -1;
   }

   *prompt_out = buf;
   return 0;
}

/* ── openai_parse_completion_request ─────────────────────────────────── */

int openai_parse_completion_request(const char *body, char *model, size_t model_n,
                                    char **prompt_out, int *stream_out)
{
   if (!body || !prompt_out)
      return -1;
   *prompt_out = NULL;
   if (stream_out)
      *stream_out = 0;

   cJSON *root = cJSON_Parse(body);
   if (!root)
      return -1;

   if (model && model_n)
      parse_model(root, model, model_n);
   if (stream_out)
      *stream_out = parse_stream(root);

   const cJSON *promptItem = cJSON_GetObjectItemCaseSensitive(root, "prompt");
   if (!cJSON_IsString(promptItem) || !promptItem->valuestring || !promptItem->valuestring[0])
   {
      cJSON_Delete(root);
      return -1;
   }

   char *dup = strdup(promptItem->valuestring);
   cJSON_Delete(root);
   if (!dup)
      return -1;

   *prompt_out = dup;
   return 0;
}

/* ── openai_parse_responses_request ──────────────────────────────────────── */

/* Append "role: text\n" to a growing heap buffer. Returns 0 on success, -1 on
 * OOM (buffer left freed and *buf NULL). Skips empty text. */
static int responses_append(char **buf, size_t *cap, size_t *len, const char *role,
                            const char *text)
{
   if (!text || !text[0])
      return 0;
   size_t need = strlen(role) + strlen(text) + 4; /* ": \n\0" */
   while (*len + need > *cap)
   {
      size_t ncap = *cap * 2;
      char *tmp = realloc(*buf, ncap);
      if (!tmp)
      {
         free(*buf);
         *buf = NULL;
         return -1;
      }
      *buf = tmp;
      *cap = ncap;
   }
   *len += (size_t)snprintf(*buf + *len, *cap - *len, "%s: %s\n", role, text);
   return 0;
}

/* Flatten one Responses `content` value (a string, or an array of parts like
 * {"type":"input_text","text":"…"}) for the given role into buf. */
static int responses_append_content(char **buf, size_t *cap, size_t *len, const char *role,
                                    const cJSON *content)
{
   if (cJSON_IsString(content))
      return responses_append(buf, cap, len, role, content->valuestring);
   if (cJSON_IsArray(content))
   {
      const cJSON *part;
      cJSON_ArrayForEach(part, content)
      {
         const cJSON *t = cJSON_GetObjectItemCaseSensitive(part, "text");
         if (cJSON_IsString(t) && responses_append(buf, cap, len, role, t->valuestring) != 0)
            return -1;
      }
   }
   return 0;
}

int openai_parse_responses_request(const char *body, char *model, size_t model_n, char **prompt_out,
                                   char *prev_id, size_t prev_id_n, int *stream_out)
{
   if (!body || !prompt_out)
      return -1;
   *prompt_out = NULL;
   if (prev_id && prev_id_n)
      prev_id[0] = '\0';
   if (stream_out)
      *stream_out = 0;

   cJSON *root = cJSON_Parse(body);
   if (!root)
      return -1;

   if (model && model_n)
      parse_model(root, model, model_n);
   if (stream_out)
      *stream_out = parse_stream(root);
   if (prev_id && prev_id_n)
   {
      const cJSON *p = cJSON_GetObjectItemCaseSensitive(root, "previous_response_id");
      if (cJSON_IsString(p) && p->valuestring)
         snprintf(prev_id, prev_id_n, "%s", p->valuestring);
   }

   const cJSON *input = cJSON_GetObjectItemCaseSensitive(root, "input");
   size_t buf_cap = 256, buf_len = 0;
   char *buf = malloc(buf_cap);
   if (!buf)
   {
      cJSON_Delete(root);
      return -1;
   }
   buf[0] = '\0';

   int ok = 1;
   if (cJSON_IsString(input))
   {
      ok = responses_append(&buf, &buf_cap, &buf_len, "user", input->valuestring) == 0;
   }
   else if (cJSON_IsArray(input))
   {
      const cJSON *item;
      cJSON_ArrayForEach(item, input)
      {
         if (cJSON_IsString(item))
         {
            if (responses_append(&buf, &buf_cap, &buf_len, "user", item->valuestring) != 0)
            {
               ok = 0;
               break;
            }
            continue;
         }
         const cJSON *roleItem = cJSON_GetObjectItemCaseSensitive(item, "role");
         const char *role = cJSON_IsString(roleItem) ? roleItem->valuestring : "user";
         const cJSON *content = cJSON_GetObjectItemCaseSensitive(item, "content");
         if (responses_append_content(&buf, &buf_cap, &buf_len, role, content) != 0)
         {
            ok = 0;
            break;
         }
      }
   }
   cJSON_Delete(root);

   if (!ok || !buf || buf_len == 0)
   {
      free(buf);
      *prompt_out = NULL;
      return -1;
   }
   *prompt_out = buf;
   return 0;
}

/* ── optional sampling-field readers ─────────────────────────────────────── */

double openai_request_double(const char *body, const char *field, double dflt, double hi)
{
   if (!body || !field)
      return dflt;
   cJSON *root = cJSON_Parse(body);
   if (!root)
      return dflt;
   double out = dflt;
   const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, field);
   if (cJSON_IsNumber(v))
   {
      double d = v->valuedouble;
      if (d >= 0.0 && d <= hi)
         out = d;
   }
   cJSON_Delete(root);
   return out;
}

int openai_request_int(const char *body, const char *field, int dflt, int hi)
{
   if (!body || !field)
      return dflt;
   cJSON *root = cJSON_Parse(body);
   if (!root)
      return dflt;
   int out = dflt;
   const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, field);
   if (cJSON_IsNumber(v))
   {
      double d = v->valuedouble;
      if (d >= 1.0 && d <= (double)hi)
         out = (int)d;
   }
   cJSON_Delete(root);
   return out;
}

int openai_request_bool(const char *body, const char *field)
{
   if (!body || !field)
      return 0;
   cJSON *root = cJSON_Parse(body);
   if (!root)
      return 0;
   const cJSON *v = cJSON_GetObjectItemCaseSensitive(root, field);
   int out = cJSON_IsTrue(v) ? 1 : 0;
   cJSON_Delete(root);
   return out;
}

/* ── embeddings ──────────────────────────────────────────────────────────── */

void openai_free_inputs(char **inputs, int n)
{
   if (!inputs)
      return;
   for (int i = 0; i < n; i++)
      free(inputs[i]);
   free(inputs);
}

int openai_parse_embeddings_request(const char *body, char *model, size_t model_n,
                                    char ***inputs_out, int *n_out)
{
   if (!inputs_out || !n_out)
      return -1;
   *inputs_out = NULL;
   *n_out = 0;
   if (!body)
      return -1;

   cJSON *root = cJSON_Parse(body);
   if (!root)
      return -1;
   if (model && model_n)
      parse_model(root, model, model_n);

   const cJSON *input = cJSON_GetObjectItemCaseSensitive(root, "input");

   /* Count how many non-empty string inputs we have. */
   int count = 0;
   if (cJSON_IsString(input) && input->valuestring && input->valuestring[0])
      count = 1;
   else if (cJSON_IsArray(input))
   {
      const cJSON *el;
      cJSON_ArrayForEach(el, input) if (cJSON_IsString(el) && el->valuestring && el->valuestring[0])
          count++;
   }
   if (count == 0)
   {
      cJSON_Delete(root);
      return -1;
   }

   char **arr = calloc((size_t)count, sizeof(char *));
   if (!arr)
   {
      cJSON_Delete(root);
      return -1;
   }
   int k = 0;
   if (cJSON_IsString(input))
   {
      arr[k++] = strdup(input->valuestring);
   }
   else
   {
      const cJSON *el;
      cJSON_ArrayForEach(el, input)
      {
         if (k >= count)
            break;
         if (cJSON_IsString(el) && el->valuestring && el->valuestring[0])
            arr[k++] = strdup(el->valuestring);
      }
   }
   cJSON_Delete(root);

   /* If any strdup failed, unwind. */
   for (int i = 0; i < k; i++)
   {
      if (!arr[i])
      {
         openai_free_inputs(arr, k);
         return -1;
      }
   }

   *inputs_out = arr;
   *n_out = k;
   return 0;
}

int openai_format_embeddings(const char *model, const float *const *vecs, const int *dims, int n,
                             int prompt_tokens, char *resp, int cap)
{
   if (!resp || cap <= 0 || n < 0 || (n > 0 && (!vecs || !dims)))
      return -1;

   cJSON *root = cJSON_CreateObject();
   if (!root)
      return -1;
   cJSON_AddStringToObject(root, "object", "list");

   cJSON *data = cJSON_CreateArray();
   if (!data)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddItemToObject(root, "data", data);

   for (int i = 0; i < n; i++)
   {
      cJSON *item = cJSON_CreateObject();
      if (!item)
      {
         cJSON_Delete(root);
         return -1;
      }
      cJSON_AddStringToObject(item, "object", "embedding");
      cJSON_AddNumberToObject(item, "index", (double)i);
      cJSON *emb = cJSON_CreateFloatArray(vecs[i], dims[i] > 0 ? dims[i] : 0);
      if (!emb)
      {
         cJSON_Delete(item);
         cJSON_Delete(root);
         return -1;
      }
      cJSON_AddItemToObject(item, "embedding", emb);
      cJSON_AddItemToArray(data, item);
   }

   cJSON_AddStringToObject(root, "model", model ? model : "");
   cJSON *usage = cJSON_CreateObject();
   if (!usage)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddItemToObject(root, "usage", usage);
   cJSON_AddNumberToObject(usage, "prompt_tokens", (double)prompt_tokens);
   cJSON_AddNumberToObject(usage, "total_tokens", (double)prompt_tokens);

   char *s = cJSON_PrintUnformatted(root);
   cJSON_Delete(root);
   if (!s)
      return -1;
   int len = snprintf(resp, (size_t)cap, "%s", s);
   free(s);
   return (len < 0 || len >= cap) ? -1 : len;
}

/* ── openai_format_models_list ────────────────────────────────────────── */

int openai_format_models_list(const char *const *ids, int n, const char *owner, char *resp, int cap)
{
   if (!resp || cap <= 0 || !ids || n <= 0)
      return -1;

   cJSON *root = cJSON_CreateObject();
   if (!root)
      return -1;

   cJSON_AddStringToObject(root, "object", "list");

   cJSON *data = cJSON_CreateArray();
   if (!data)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddItemToObject(root, "data", data);

   for (int i = 0; i < n; i++)
   {
      cJSON *item = cJSON_CreateObject();
      if (!item)
      {
         cJSON_Delete(root);
         return -1;
      }
      cJSON_AddStringToObject(item, "id", ids[i] ? ids[i] : "");
      cJSON_AddStringToObject(item, "object", "model");
      cJSON_AddStringToObject(item, "owned_by", owner ? owner : "");
      cJSON_AddItemToArray(data, item);
   }

   char *s = cJSON_PrintUnformatted(root);
   cJSON_Delete(root);
   if (!s)
      return -1;

   int len = snprintf(resp, (size_t)cap, "%s", s);
   free(s);
   return (len < 0 || len >= cap) ? -1 : len;
}

/* ── openai_format_chat_completion ────────────────────────────────────── */

int openai_format_chat_completion(const char *id, const char *model, const char *content,
                                  long created, int prompt_tokens, int completion_tokens,
                                  char *resp, int cap)
{
   if (!resp || cap <= 0)
      return -1;

   cJSON *root = cJSON_CreateObject();
   if (!root)
      return -1;

   cJSON_AddStringToObject(root, "id", id ? id : "");
   cJSON_AddStringToObject(root, "object", "chat.completion");
   cJSON_AddNumberToObject(root, "created", (double)created);
   cJSON_AddStringToObject(root, "model", model ? model : "");

   cJSON *choices = cJSON_CreateArray();
   if (!choices)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddItemToObject(root, "choices", choices);

   cJSON *choice = cJSON_CreateObject();
   if (!choice)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddNumberToObject(choice, "index", 0.0);
   cJSON_AddItemToArray(choices, choice);

   cJSON *msg = cJSON_CreateObject();
   if (!msg)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddStringToObject(msg, "role", "assistant");
   cJSON_AddStringToObject(msg, "content", content ? content : "");
   cJSON_AddItemToObject(choice, "message", msg);

   cJSON_AddStringToObject(choice, "finish_reason", "stop");

   cJSON *usage = cJSON_CreateObject();
   if (!usage)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddItemToObject(root, "usage", usage);
   cJSON_AddNumberToObject(usage, "prompt_tokens", (double)prompt_tokens);
   cJSON_AddNumberToObject(usage, "completion_tokens", (double)completion_tokens);
   cJSON_AddNumberToObject(usage, "total_tokens", (double)(prompt_tokens + completion_tokens));

   char *s = cJSON_PrintUnformatted(root);
   cJSON_Delete(root);
   if (!s)
      return -1;

   int len = snprintf(resp, (size_t)cap, "%s", s);
   free(s);
   return (len < 0 || len >= cap) ? -1 : len;
}

/* ── openai_format_chat_chunk (streaming SSE delta) ──────────────────────── */

int openai_format_chat_chunk(const char *id, const char *model, long created, int role,
                             const char *delta_content, int finish, char *resp, int cap)
{
   if (!resp || cap <= 0)
      return -1;

   cJSON *root = cJSON_CreateObject();
   if (!root)
      return -1;

   cJSON_AddStringToObject(root, "id", id ? id : "");
   cJSON_AddStringToObject(root, "object", "chat.completion.chunk");
   cJSON_AddNumberToObject(root, "created", (double)created);
   cJSON_AddStringToObject(root, "model", model ? model : "");

   cJSON *choices = cJSON_CreateArray();
   cJSON *choice = cJSON_CreateObject();
   cJSON *delta = cJSON_CreateObject();
   if (!choices || !choice || !delta)
   {
      cJSON_Delete(root);
      cJSON_Delete(choices);
      cJSON_Delete(choice);
      cJSON_Delete(delta);
      return -1;
   }
   cJSON_AddItemToObject(root, "choices", choices);
   cJSON_AddItemToArray(choices, choice);
   cJSON_AddNumberToObject(choice, "index", 0.0);
   cJSON_AddItemToObject(choice, "delta", delta);

   /* The first frame announces the assistant role; content frames carry the
    * text delta; the terminal frame sets finish_reason and an empty delta. */
   if (role)
      cJSON_AddStringToObject(delta, "role", "assistant");
   if (delta_content)
      cJSON_AddStringToObject(delta, "content", delta_content);

   if (finish)
      cJSON_AddStringToObject(choice, "finish_reason", "stop");
   else
      cJSON_AddNullToObject(choice, "finish_reason");

   char *s = cJSON_PrintUnformatted(root);
   cJSON_Delete(root);
   if (!s)
      return -1;
   int len = snprintf(resp, (size_t)cap, "%s", s);
   free(s);
   return (len < 0 || len >= cap) ? -1 : len;
}

/* ── openai_format_text_chunk (legacy completions streaming) ─────────────── */

int openai_format_text_chunk(const char *id, const char *model, long created,
                             const char *text_delta, int finish, char *resp, int cap)
{
   if (!resp || cap <= 0)
      return -1;

   cJSON *root = cJSON_CreateObject();
   if (!root)
      return -1;

   cJSON_AddStringToObject(root, "id", id ? id : "");
   cJSON_AddStringToObject(root, "object", "text_completion");
   cJSON_AddNumberToObject(root, "created", (double)created);
   cJSON_AddStringToObject(root, "model", model ? model : "");

   cJSON *choices = cJSON_CreateArray();
   cJSON *choice = cJSON_CreateObject();
   if (!choices || !choice)
   {
      cJSON_Delete(root);
      cJSON_Delete(choices);
      cJSON_Delete(choice);
      return -1;
   }
   cJSON_AddItemToObject(root, "choices", choices);
   cJSON_AddItemToArray(choices, choice);
   cJSON_AddStringToObject(choice, "text", text_delta ? text_delta : "");
   cJSON_AddNumberToObject(choice, "index", 0.0);
   if (finish)
      cJSON_AddStringToObject(choice, "finish_reason", "stop");
   else
      cJSON_AddNullToObject(choice, "finish_reason");

   char *s = cJSON_PrintUnformatted(root);
   cJSON_Delete(root);
   if (!s)
      return -1;
   int len = snprintf(resp, (size_t)cap, "%s", s);
   free(s);
   return (len < 0 || len >= cap) ? -1 : len;
}

/* ── openai_format_text_completion ───────────────────────────────────── */

int openai_format_text_completion(const char *id, const char *model, const char *content,
                                  long created, int prompt_tokens, int completion_tokens,
                                  char *resp, int cap)
{
   if (!resp || cap <= 0)
      return -1;

   cJSON *root = cJSON_CreateObject();
   if (!root)
      return -1;

   cJSON_AddStringToObject(root, "id", id ? id : "");
   cJSON_AddStringToObject(root, "object", "text_completion");
   cJSON_AddNumberToObject(root, "created", (double)created);
   cJSON_AddStringToObject(root, "model", model ? model : "");

   cJSON *choices = cJSON_CreateArray();
   if (!choices)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddItemToObject(root, "choices", choices);

   cJSON *choice = cJSON_CreateObject();
   if (!choice)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddNumberToObject(choice, "index", 0.0);
   cJSON_AddItemToArray(choices, choice);

   cJSON_AddStringToObject(choice, "text", content ? content : "");
   cJSON_AddStringToObject(choice, "finish_reason", "stop");

   cJSON *usage = cJSON_CreateObject();
   if (!usage)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddItemToObject(root, "usage", usage);
   cJSON_AddNumberToObject(usage, "prompt_tokens", (double)prompt_tokens);
   cJSON_AddNumberToObject(usage, "completion_tokens", (double)completion_tokens);
   cJSON_AddNumberToObject(usage, "total_tokens", (double)(prompt_tokens + completion_tokens));

   char *s = cJSON_PrintUnformatted(root);
   cJSON_Delete(root);
   if (!s)
      return -1;

   int len = snprintf(resp, (size_t)cap, "%s", s);
   free(s);
   return (len < 0 || len >= cap) ? -1 : len;
}

/* ── openai_format_response (Responses API) ──────────────────────────────── */

/* Build a `response` object (caller owns the returned cJSON, or NULL on OOM).
 * status is e.g. "completed" or "in_progress". When with_output != 0 the
 * assistant output message + content part + usage are included; otherwise the
 * output array is empty and usage is omitted (the shape of an in-progress
 * response just after creation). */
static cJSON *build_response_object(const char *id, const char *model, const char *output_text,
                                    long created, int prompt_tokens, int completion_tokens,
                                    const char *status, int with_output, const char *object_type)
{
   cJSON *root = cJSON_CreateObject();
   if (!root)
      return NULL;

   cJSON_AddStringToObject(root, "id", id ? id : "");
   cJSON_AddStringToObject(root, "object", object_type ? object_type : "response");
   cJSON_AddNumberToObject(root, "created_at", (double)created);
   cJSON_AddStringToObject(root, "model", model ? model : "");
   cJSON_AddStringToObject(root, "status", status ? status : "completed");

   cJSON *output = cJSON_AddArrayToObject(root, "output");
   if (!output)
   {
      cJSON_Delete(root);
      return NULL;
   }
   if (!with_output)
      return root;

   cJSON *msg = cJSON_CreateObject();
   if (!msg)
   {
      cJSON_Delete(root);
      return NULL;
   }
   cJSON_AddItemToArray(output, msg);
   char msg_id[64];
   snprintf(msg_id, sizeof(msg_id), "%s-msg", id ? id : "resp");
   cJSON_AddStringToObject(msg, "id", msg_id);
   cJSON_AddStringToObject(msg, "type", "message");
   cJSON_AddStringToObject(msg, "status", "completed");
   cJSON_AddStringToObject(msg, "role", "assistant");

   cJSON *content = cJSON_AddArrayToObject(msg, "content");
   cJSON *part = cJSON_CreateObject();
   if (!content || !part)
   {
      cJSON_Delete(part);
      cJSON_Delete(root);
      return NULL;
   }
   cJSON_AddItemToArray(content, part);
   cJSON_AddStringToObject(part, "type", "output_text");
   cJSON_AddStringToObject(part, "text", output_text ? output_text : "");
   cJSON_AddItemToObject(part, "annotations", cJSON_CreateArray());

   cJSON *usage = cJSON_AddObjectToObject(root, "usage");
   if (usage)
   {
      cJSON_AddNumberToObject(usage, "input_tokens", (double)prompt_tokens);
      cJSON_AddNumberToObject(usage, "output_tokens", (double)completion_tokens);
      cJSON_AddNumberToObject(usage, "total_tokens", (double)(prompt_tokens + completion_tokens));
   }
   return root;
}

/* Print a (possibly wrapped) cJSON object into resp[cap], deleting it. */
static int emit_json(cJSON *root, char *resp, int cap)
{
   if (!root)
      return -1;
   char *s = cJSON_PrintUnformatted(root);
   cJSON_Delete(root);
   if (!s)
      return -1;
   int len = snprintf(resp, (size_t)cap, "%s", s);
   free(s);
   return (len < 0 || len >= cap) ? -1 : len;
}

int openai_format_response(const char *id, const char *model, const char *output_text, long created,
                           int prompt_tokens, int completion_tokens, char *resp, int cap)
{
   if (!resp || cap <= 0)
      return -1;
   cJSON *root = build_response_object(id, model, output_text, created, prompt_tokens,
                                       completion_tokens, "completed", 1, "response");
   return emit_json(root, resp, cap);
}

int openai_format_run(const char *id, const char *model, const char *output_text, long created,
                      int prompt_tokens, int completion_tokens, const char *status, char *resp,
                      int cap)
{
   if (!resp || cap <= 0)
      return -1;
   cJSON *root = build_response_object(id, model, output_text, created, prompt_tokens,
                                       completion_tokens, status ? status : "completed", 1, "run");
   return emit_json(root, resp, cap);
}

/* ── Responses API streaming events ──────────────────────────────────────── */

int openai_format_responses_created(const char *id, const char *model, long created, char *resp,
                                    int cap)
{
   if (!resp || cap <= 0)
      return -1;
   cJSON *root = cJSON_CreateObject();
   if (!root)
      return -1;
   cJSON_AddStringToObject(root, "type", "response.created");
   cJSON *obj = build_response_object(id, model, "", created, 0, 0, "in_progress", 0, "response");
   if (!obj)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddItemToObject(root, "response", obj);
   return emit_json(root, resp, cap);
}

int openai_format_responses_delta(const char *item_id, const char *delta, char *resp, int cap)
{
   if (!resp || cap <= 0)
      return -1;
   cJSON *root = cJSON_CreateObject();
   if (!root)
      return -1;
   cJSON_AddStringToObject(root, "type", "response.output_text.delta");
   cJSON_AddStringToObject(root, "item_id", item_id ? item_id : "");
   cJSON_AddNumberToObject(root, "output_index", 0.0);
   cJSON_AddNumberToObject(root, "content_index", 0.0);
   cJSON_AddStringToObject(root, "delta", delta ? delta : "");
   return emit_json(root, resp, cap);
}

int openai_format_responses_completed(const char *id, const char *model, const char *output_text,
                                      long created, int prompt_tokens, int completion_tokens,
                                      char *resp, int cap)
{
   if (!resp || cap <= 0)
      return -1;
   cJSON *root = cJSON_CreateObject();
   if (!root)
      return -1;
   cJSON_AddStringToObject(root, "type", "response.completed");
   cJSON *obj = build_response_object(id, model, output_text, created, prompt_tokens,
                                      completion_tokens, "completed", 1, "response");
   if (!obj)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddItemToObject(root, "response", obj);
   return emit_json(root, resp, cap);
}

/* ── openai_format_error ──────────────────────────────────────────────── */

int openai_format_error(char *resp, int cap, const char *type, const char *message)
{
   if (!resp || cap <= 0)
      return -1;

   cJSON *root = cJSON_CreateObject();
   if (!root)
      return -1;

   cJSON *err = cJSON_CreateObject();
   if (!err)
   {
      cJSON_Delete(root);
      return -1;
   }
   cJSON_AddItemToObject(root, "error", err);
   cJSON_AddStringToObject(err, "message", message ? message : "");
   cJSON_AddStringToObject(err, "type", type ? type : "");

   char *s = cJSON_PrintUnformatted(root);
   cJSON_Delete(root);
   if (!s)
      return -1;

   int len = snprintf(resp, (size_t)cap, "%s", s);
   free(s);
   return (len < 0 || len >= cap) ? -1 : len;
}