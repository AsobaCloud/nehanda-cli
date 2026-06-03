/* openai_chat.c: inference-backed handlers for the OpenAI-compatible
 * /v1/chat/completions and /v1/completions routes.
 *
 * Registered into the server_http dispatch via server_http_set_*_handler at
 * server startup (openai_chat_register). Keeping the inference here — rather
 * than in server_http.c — keeps the agent/provider dependency closure out of
 * the server_http translation unit and its unit test (which injects a stub
 * handler instead).
 *
 * /v1/chat/completions supports SSE streaming via chat_stream_handler (the
 * server_http stream seam); /v1/completions and /v1/responses are still
 * non-streaming and reject "stream":true. Streaming computes the full
 * completion, then emits it as chat.completion.chunk deltas (provider-
 * incremental token streaming is a follow-up). The handler runs on the HTTP
 * listener thread, which serves one connection at a time, so a completion
 * blocks that listener for its duration — acceptable for the local
 * single-owner surface this first cut targets. */
#include "server_http.h"
#include "openai_shape.h"
#include "openai_responses_store.h" /* previous_response_id continuation store */
#include "openai_runs_store.h"      /* GET /v1/runs/{id} record store */
#include "aimee.h"                  /* EMBED_MAX_DIM, MAX_PATH_LEN (used by agent_types.h below) */
#include "config.h"                 /* config_t, config_load */
#include "agent_config.h"
#include "agent_exec.h"
#include "agent_tools.h" /* agent_tools_set_tool_event_cb — /v1/runs tool events */
#include "agent_types.h"
#include "memory.h" /* memory_embed_text */
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OPENAI_CHAT_MAX_TOKENS  2048
#define OPENAI_CHAT_TEMPERATURE 0.7

/* Cap on the running /v1/responses transcript carried between turns via
 * previous_response_id. Bounds memory and the prompt we feed back to the agent. */
#define OPENAI_RESP_TRANSCRIPT_MAX (32 * 1024)

/* Shared body for both endpoints. chat != 0 selects the chat.completion shape
 * (and chat-request parse); otherwise the legacy text_completion shape. */
static int run_completion(int chat, const char *body, char *resp, int cap)
{
   char model[64] = "";
   char *prompt = NULL;
   int stream = 0;
   int prc = chat ? openai_parse_chat_request(body, model, sizeof(model), &prompt, &stream)
                  : openai_parse_completion_request(body, model, sizeof(model), &prompt, &stream);
   if (prc != 0)
   {
      openai_format_error(resp, cap, "invalid_request_error",
                          chat ? "invalid chat completion request: expected messages[] with content"
                               : "invalid completion request: expected a non-empty prompt");
      return 400;
   }
   if (stream)
   {
      free(prompt);
      openai_format_error(resp, cap, "invalid_request_error",
                          "streaming responses are not yet supported on this endpoint");
      return 400;
   }

   agent_config_t acfg;
   if (agent_load_config(&acfg) != 0)
   {
      free(prompt);
      openai_format_error(resp, cap, "server_error", "no agent configuration available");
      return 503;
   }
   /* Honour the requested model: "aimee" (or empty) means the default agent;
    * any other value selects a configured agent by name, falling back to the
    * default when it doesn't match one. */
   agent_t *ag = NULL;
   if (model[0] && strcmp(model, "aimee") != 0)
      ag = agent_find(&acfg, model);
   if (!ag)
      ag = agent_find(&acfg, acfg.default_agent);
   if (!ag && acfg.agent_count > 0)
      ag = &acfg.agents[0];
   if (!ag)
   {
      free(prompt);
      openai_format_error(resp, cap, "server_error", "no agent configured");
      return 503;
   }

   /* Honour the OpenAI sampling params when present, else fall back to the
    * endpoint defaults. temperature clamps to [0, 2]; max_tokens to [1, hi]. */
   double temperature = openai_request_double(body, "temperature", OPENAI_CHAT_TEMPERATURE, 2.0);
   int max_tokens = openai_request_int(body, "max_tokens", OPENAI_CHAT_MAX_TOKENS, 32768);

   agent_result_t result;
   memset(&result, 0, sizeof(result));
   int erc = agent_execute(ag, NULL, prompt, max_tokens, temperature, &result);
   free(prompt);

   if (erc != 0 || !result.response)
   {
      openai_format_error(resp, cap, "upstream_error",
                          result.error[0] ? result.error : "completion failed");
      free(result.response);
      return 502;
   }

   long created = (long)time(NULL);
   char id[48];
   snprintf(id, sizeof(id), "%s-%ld", chat ? "chatcmpl" : "cmpl", created);

   int len = chat ? openai_format_chat_completion(id, model, result.response, created,
                                                  result.prompt_tokens, result.completion_tokens,
                                                  resp, cap)
                  : openai_format_text_completion(id, model, result.response, created,
                                                  result.prompt_tokens, result.completion_tokens,
                                                  resp, cap);
   free(result.response);
   if (len < 0)
   {
      openai_format_error(resp, cap, "server_error", "response did not fit the buffer");
      return 500;
   }
   return 200;
}

static int chat_completions_handler(const char *body, char *resp, int cap)
{
   return run_completion(1, body, resp, cap);
}

static int completions_handler(const char *body, char *resp, int cap)
{
   return run_completion(0, body, resp, cap);
}

/* GET /v1/models provider: copy each configured agent name (capped at max) so
 * clients can target a specific (provider,model) binding via the `model`
 * field. Names are copied into the caller's slots — no escaping pointers. */
static int models_provider(char ids[][SERVER_HTTP_MODEL_ID_MAX], int max)
{
   agent_config_t acfg;
   if (agent_load_config(&acfg) != 0)
      return 0;
   int k = 0;
   for (int i = 0; i < acfg.agent_count && k < max; i++)
   {
      if (!acfg.agents[i].name[0] || strcmp(acfg.agents[i].name, "aimee") == 0)
         continue; /* "aimee" is already advertised by the route */
      snprintf(ids[k], SERVER_HTTP_MODEL_ID_MAX, "%s", acfg.agents[i].name);
      k++;
   }
   return k;
}

/* POST /v1/embeddings: embed each input via the configured embedder
 * (memory_embed_text) and shape the OpenAI embeddings list. Returns 502 when
 * the embedder is unavailable (e.g. the sidecar isn't running). */
static int embeddings_handler(const char *body, char *resp, int cap)
{
   char model[64] = "";
   char **inputs = NULL;
   int n = 0;
   if (openai_parse_embeddings_request(body, model, sizeof(model), &inputs, &n) != 0)
   {
      openai_format_error(resp, cap, "invalid_request_error",
                          "invalid embeddings request: expected `input` string or array");
      return 400;
   }

   config_t cfg;
   memset(&cfg, 0, sizeof(cfg));
   config_load(&cfg);
   const char *cmd = cfg.embedding_command[0] ? cfg.embedding_command : "builtin";

   float **vecs = calloc((size_t)n, sizeof(float *));
   int *dims = calloc((size_t)n, sizeof(int));
   if (!vecs || !dims)
   {
      free(vecs);
      free(dims);
      openai_free_inputs(inputs, n);
      openai_format_error(resp, cap, "server_error", "out of memory");
      return 500;
   }

   int ok = 1;
   int prompt_chars = 0;
   for (int i = 0; i < n; i++)
   {
      vecs[i] = malloc(sizeof(float) * EMBED_MAX_DIM);
      if (!vecs[i])
      {
         ok = 0;
         break;
      }
      int d = memory_embed_text(inputs[i], cmd, vecs[i], EMBED_MAX_DIM);
      if (d <= 0)
      {
         ok = 0;
         break;
      }
      dims[i] = d;
      prompt_chars += (int)strlen(inputs[i]);
   }

   int status;
   if (!ok)
   {
      openai_format_error(resp, cap, "upstream_error",
                          "embedding generation failed (is the embedder available?)");
      status = 502;
   }
   else
   {
      /* ~4 chars/token is the usual OpenAI rule of thumb. */
      int len = openai_format_embeddings(model, (const float *const *)vecs, dims, n,
                                         prompt_chars / 4, resp, cap);
      if (len < 0)
      {
         openai_format_error(resp, cap, "server_error",
                             "embeddings response did not fit the buffer");
         status = 500;
      }
      else
         status = 200;
   }

   for (int i = 0; i < n; i++)
      free(vecs[i]);
   free(vecs);
   free(dims);
   openai_free_inputs(inputs, n);
   return status;
}

/* Unique /v1/responses id (the listener serves one connection at a time, so the
 * counter needs no lock); distinct ids let a client chain turns even within the
 * same wall-clock second. Shared by the unary and streaming handlers. */
static atomic_ulong g_resp_seq = 0;
static void responses_mint_id(long created, char *buf, size_t n)
{
   snprintf(buf, n, "resp_%ld_%lu", created, atomic_fetch_add(&g_resp_seq, 1) + 1);
}

/* Persist the running transcript (full prompt + assistant reply) under id so a
 * follow-up carrying it as previous_response_id continues the conversation. */
static void responses_store_turn(const char *id, const char *full, const char *response)
{
   size_t need = strlen(full) + sizeof("\nassistant: ") + strlen(response) + 1;
   char *transcript = malloc(need);
   if (transcript)
   {
      snprintf(transcript, need, "%s\nassistant: %s", full, response);
      openai_responses_store_put(id, transcript);
      free(transcript);
   }
}

/* POST /v1/responses: the OpenAI Responses API (non-streaming). Runs inference
 * on the flattened `input` like chat/completions and shapes a `response`
 * object. When `previous_response_id` names a prior turn (held in the
 * in-process responses store), its accumulated transcript is prepended so the
 * model continues the conversation; the new turn (prior transcript + this input
 * + the assistant reply) is stored under the freshly minted response id so a
 * follow-up can chain off it in turn. The store is in-process only (not durable
 * across restarts), matching this surface's local single-owner posture. */
static int responses_handler(const char *body, char *resp, int cap)
{
   char model[64] = "";
   char prev_id[128] = "";
   char *prompt = NULL;
   int stream = 0;
   if (openai_parse_responses_request(body, model, sizeof(model), &prompt, prev_id, sizeof(prev_id),
                                      &stream) != 0)
   {
      openai_format_error(resp, cap, "invalid_request_error",
                          "invalid responses request: expected non-empty `input`");
      return 400;
   }
   if (stream)
   {
      free(prompt);
      openai_format_error(resp, cap, "invalid_request_error",
                          "streaming responses are not yet supported on this endpoint");
      return 400;
   }

   agent_config_t acfg;
   if (agent_load_config(&acfg) != 0)
   {
      free(prompt);
      openai_format_error(resp, cap, "server_error", "no agent configuration available");
      return 503;
   }
   agent_t *ag = NULL;
   if (model[0] && strcmp(model, "aimee") != 0)
      ag = agent_find(&acfg, model);
   if (!ag)
      ag = agent_find(&acfg, acfg.default_agent);
   if (!ag && acfg.agent_count > 0)
      ag = &acfg.agents[0];
   if (!ag)
   {
      free(prompt);
      openai_format_error(resp, cap, "server_error", "no agent configured");
      return 503;
   }

   double temperature = openai_request_double(body, "temperature", OPENAI_CHAT_TEMPERATURE, 2.0);
   int max_tokens = openai_request_int(body, "max_tokens", OPENAI_CHAT_MAX_TOKENS, 32768);

   /* Continuation: if previous_response_id names a stored conversation, prepend
    * its transcript so this turn continues it. `full` points at either the bare
    * input or the heap-allocated combined transcript (freed below). */
   char *full = prompt;
   char *combined = NULL;
   char prev[OPENAI_RESP_TRANSCRIPT_MAX];
   if (prev_id[0] && openai_responses_store_get(prev_id, prev, sizeof(prev)) && prev[0])
   {
      size_t need = strlen(prev) + 1 + strlen(prompt) + 1;
      combined = malloc(need);
      if (combined)
      {
         snprintf(combined, need, "%s\n%s", prev, prompt);
         full = combined;
      }
   }

   agent_result_t result;
   memset(&result, 0, sizeof(result));
   int erc = agent_execute(ag, NULL, full, max_tokens, temperature, &result);

   if (erc != 0 || !result.response)
   {
      openai_format_error(resp, cap, "upstream_error",
                          result.error[0] ? result.error : "response failed");
      free(result.response);
      free(prompt);
      free(combined);
      return 502;
   }

   long created = (long)time(NULL);
   char id[64];
   responses_mint_id(created, id, sizeof(id));
   responses_store_turn(id, full, result.response);
   free(prompt);
   free(combined);

   int len = openai_format_response(id, model, result.response, created, result.prompt_tokens,
                                    result.completion_tokens, resp, cap);
   free(result.response);
   if (len < 0)
   {
      openai_format_error(resp, cap, "server_error", "response did not fit the buffer");
      return 500;
   }
   return 200;
}

/* SSE streaming for POST /v1/chat/completions (stream:true). First cut:
 * compute the full completion (non-incremental) and emit it as OpenAI
 * chat.completion.chunk frames — a role frame, the content split into deltas,
 * and a terminal finish frame. server_http wraps each frame as `data: …` and
 * appends `data: [DONE]`. Provider-incremental token streaming is a follow-up;
 * this already lets OpenAI streaming clients work end-to-end. */
#define OPENAI_STREAM_CHUNK 80

static void emit_chunk(server_http_sse_emit emit, void *ctx, const char *id, const char *model,
                       long created, int role, const char *content, int finish)
{
   char frame[1024];
   if (openai_format_chat_chunk(id, model, created, role, content, finish, frame, sizeof(frame)) >
       0)
      emit(ctx, frame);
}

static void emit_text_chunk(server_http_sse_emit emit, void *ctx, const char *id, const char *model,
                            long created, const char *text, int finish)
{
   char frame[1024];
   if (openai_format_text_chunk(id, model, created, text, finish, frame, sizeof(frame)) > 0)
      emit(ctx, frame);
}

/* Resolve the agent for `model` into *acfg (caller-owned, must outlive the
 * returned pointer — it indexes into acfg). Returns NULL when none configured. */
static agent_t *stream_pick_agent(agent_config_t *acfg, const char *model)
{
   if (agent_load_config(acfg) != 0)
      return NULL;
   agent_t *ag = NULL;
   if (model[0] && strcmp(model, "aimee") != 0)
      ag = agent_find(acfg, model);
   if (!ag)
      ag = agent_find(acfg, acfg->default_agent);
   if (!ag && acfg->agent_count > 0)
      ag = &acfg->agents[0];
   return ag;
}

static int chat_stream_handler(const char *body, server_http_sse_emit emit, void *ctx)
{
   char model[64] = "";
   char *prompt = NULL;
   int stream = 0;
   long created = (long)time(NULL);
   char id[48];
   snprintf(id, sizeof(id), "chatcmpl-%ld", created);

   if (openai_parse_chat_request(body, model, sizeof(model), &prompt, &stream) != 0)
   {
      emit_chunk(emit, ctx, id, model, created, 1, "invalid chat completion request", 0);
      emit_chunk(emit, ctx, id, model, created, 0, NULL, 1);
      free(prompt);
      return 0;
   }

   agent_config_t acfg;
   agent_t *ag = stream_pick_agent(&acfg, model);
   if (!ag)
   {
      emit_chunk(emit, ctx, id, model, created, 1, "no agent configured", 0);
      emit_chunk(emit, ctx, id, model, created, 0, NULL, 1);
      free(prompt);
      return 0;
   }

   double temperature = openai_request_double(body, "temperature", OPENAI_CHAT_TEMPERATURE, 2.0);
   int max_tokens = openai_request_int(body, "max_tokens", OPENAI_CHAT_MAX_TOKENS, 32768);

   agent_result_t result;
   memset(&result, 0, sizeof(result));
   int erc = agent_execute(ag, NULL, prompt, max_tokens, temperature, &result);
   free(prompt);

   emit_chunk(emit, ctx, id, model, created, 1, NULL, 0); /* role frame */
   if (erc != 0 || !result.response)
   {
      emit_chunk(emit, ctx, id, model, created, 0,
                 result.error[0] ? result.error : "completion failed", 0);
   }
   else
   {
      const char *txt = result.response;
      size_t n = strlen(txt);
      for (size_t off = 0; off < n; off += OPENAI_STREAM_CHUNK)
      {
         char seg[OPENAI_STREAM_CHUNK + 1];
         size_t take = (n - off < OPENAI_STREAM_CHUNK) ? (n - off) : OPENAI_STREAM_CHUNK;
         memcpy(seg, txt + off, take);
         seg[take] = '\0';
         emit_chunk(emit, ctx, id, model, created, 0, seg, 0);
      }
   }
   emit_chunk(emit, ctx, id, model, created, 0, NULL, 1); /* finish frame */
   free(result.response);
   return 0;
}

/* SSE streaming for POST /v1/completions (legacy text_completion). Same
 * compute-then-chunk shape as chat, emitting text_completion chunk frames. */
static int completion_stream_handler(const char *body, server_http_sse_emit emit, void *ctx)
{
   char model[64] = "";
   char *prompt = NULL;
   int stream = 0;
   long created = (long)time(NULL);
   char id[48];
   snprintf(id, sizeof(id), "cmpl-%ld", created);

   if (openai_parse_completion_request(body, model, sizeof(model), &prompt, &stream) != 0)
   {
      emit_text_chunk(emit, ctx, id, model, created, "invalid completion request", 0);
      emit_text_chunk(emit, ctx, id, model, created, "", 1);
      free(prompt);
      return 0;
   }

   agent_config_t acfg;
   agent_t *ag = stream_pick_agent(&acfg, model);
   if (!ag)
   {
      emit_text_chunk(emit, ctx, id, model, created, "no agent configured", 0);
      emit_text_chunk(emit, ctx, id, model, created, "", 1);
      free(prompt);
      return 0;
   }

   double temperature = openai_request_double(body, "temperature", OPENAI_CHAT_TEMPERATURE, 2.0);
   int max_tokens = openai_request_int(body, "max_tokens", OPENAI_CHAT_MAX_TOKENS, 32768);

   agent_result_t result;
   memset(&result, 0, sizeof(result));
   int erc = agent_execute(ag, NULL, prompt, max_tokens, temperature, &result);
   free(prompt);

   if (erc != 0 || !result.response)
   {
      emit_text_chunk(emit, ctx, id, model, created,
                      result.error[0] ? result.error : "completion failed", 0);
   }
   else
   {
      const char *txt = result.response;
      size_t n = strlen(txt);
      for (size_t off = 0; off < n; off += OPENAI_STREAM_CHUNK)
      {
         char seg[OPENAI_STREAM_CHUNK + 1];
         size_t take = (n - off < OPENAI_STREAM_CHUNK) ? (n - off) : OPENAI_STREAM_CHUNK;
         memcpy(seg, txt + off, take);
         seg[take] = '\0';
         emit_text_chunk(emit, ctx, id, model, created, seg, 0);
      }
   }
   emit_text_chunk(emit, ctx, id, model, created, "", 1); /* finish frame */
   free(result.response);
   return 0;
}

/* SSE streaming for POST /v1/responses (stream:true). The Responses API uses
 * typed events: a `response.created`, then `response.output_text.delta` events
 * carrying the text (chunked), then a terminal `response.completed` with the
 * full response object. Honours previous_response_id continuation and persists
 * the turn just like the unary handler. Compute-then-chunk first cut. */
static int responses_stream_handler(const char *body, server_http_sse_event_emit emit, void *ctx)
{
   char model[64] = "";
   char prev_id[128] = "";
   char *prompt = NULL;
   int stream = 0;
   long created = (long)time(NULL);
   char id[64];
   responses_mint_id(created, id, sizeof(id));
   char frame[2048];

   if (openai_parse_responses_request(body, model, sizeof(model), &prompt, prev_id, sizeof(prev_id),
                                      &stream) != 0)
   {
      if (openai_format_responses_created(id, model, created, frame, sizeof(frame)) > 0)
         emit(ctx, "response.created", frame);
      if (openai_format_responses_completed(id, model, "invalid responses request", created, 0, 0,
                                            frame, sizeof(frame)) > 0)
         emit(ctx, "response.completed", frame);
      free(prompt);
      return 0;
   }

   agent_config_t acfg;
   agent_t *ag = stream_pick_agent(&acfg, model);
   if (!ag)
   {
      if (openai_format_responses_created(id, model, created, frame, sizeof(frame)) > 0)
         emit(ctx, "response.created", frame);
      if (openai_format_responses_completed(id, model, "no agent configured", created, 0, 0, frame,
                                            sizeof(frame)) > 0)
         emit(ctx, "response.completed", frame);
      free(prompt);
      return 0;
   }

   double temperature = openai_request_double(body, "temperature", OPENAI_CHAT_TEMPERATURE, 2.0);
   int max_tokens = openai_request_int(body, "max_tokens", OPENAI_CHAT_MAX_TOKENS, 32768);

   /* Continuation: prepend a prior turn's transcript when chaining. */
   char *full = prompt;
   char *combined = NULL;
   char prev[OPENAI_RESP_TRANSCRIPT_MAX];
   if (prev_id[0] && openai_responses_store_get(prev_id, prev, sizeof(prev)) && prev[0])
   {
      size_t need = strlen(prev) + 1 + strlen(prompt) + 1;
      combined = malloc(need);
      if (combined)
      {
         snprintf(combined, need, "%s\n%s", prev, prompt);
         full = combined;
      }
   }

   agent_result_t result;
   memset(&result, 0, sizeof(result));
   int erc = agent_execute(ag, NULL, full, max_tokens, temperature, &result);

   if (openai_format_responses_created(id, model, created, frame, sizeof(frame)) > 0)
      emit(ctx, "response.created", frame);

   char item_id[72];
   snprintf(item_id, sizeof(item_id), "%s-msg", id);
   const char *txt = (erc == 0 && result.response)
                         ? result.response
                         : (result.error[0] ? result.error : "response failed");

   size_t n = strlen(txt);
   for (size_t off = 0; off < n; off += OPENAI_STREAM_CHUNK)
   {
      char seg[OPENAI_STREAM_CHUNK + 1];
      size_t take = (n - off < OPENAI_STREAM_CHUNK) ? (n - off) : OPENAI_STREAM_CHUNK;
      memcpy(seg, txt + off, take);
      seg[take] = '\0';
      char dframe[OPENAI_STREAM_CHUNK * 6 + 256];
      if (openai_format_responses_delta(item_id, seg, dframe, sizeof(dframe)) > 0)
         emit(ctx, "response.output_text.delta", dframe);
   }

   if (erc == 0 && result.response)
      responses_store_turn(id, full, result.response);

   /* Terminal completed event carries the full output text — size for it. */
   size_t cn = n + 512;
   char *cframe = malloc(cn);
   if (cframe)
   {
      if (openai_format_responses_completed(id, model, txt, created, result.prompt_tokens,
                                            result.completion_tokens, cframe, (int)cn) > 0)
         emit(ctx, "response.completed", cframe);
      free(cframe);
   }

   free(result.response);
   free(prompt);
   free(combined);
   return 0;
}

/* Async /v1/runs execution. POST enqueues a background worker and returns the
 * `queued` run immediately; the worker runs inference off the listener thread,
 * publishes live events + status into the runs store, and honors cancellation
 * at the step boundary. There is no provider-incremental token streaming yet, so
 * output deltas are emitted in one batch once the (blocking) step returns —
 * still off-thread, and preceded by a live `response.in_progress` event so
 * subscribers see progress before completion. */
#define RUN_JSON_CAP (256 * 1024)
#define RUN_DELTA    80

typedef struct
{
   char run_id[64];
   char model[64];
   char *prompt; /* heap; owned by the worker, freed there */
   double temperature;
   int max_tokens;
   long created;
} run_job_t;

/* Build a run object with no output text at the given status into buf. Returns
 * the pointer (buf) on success or "{}" if it does not fit. */
static const char *run_status_json(const run_job_t *j, const char *status, char *buf, int cap)
{
   return openai_format_run(j->run_id, j->model, "", j->created, 0, 0, status, buf, cap) > 0 ? buf
                                                                                             : "{}";
}

/* Tool-event hook for /v1/runs: append a `tool_call.started` /
 * `tool_call.completed` event (with the tool name) to the run's event stream as
 * each tool fires during the (blocking) turn. ud is the run_id. */
static void runs_tool_event_cb(const char *phase, const char *name, void *ud)
{
   const char *run_id = (const char *)ud;
   char ev[48];
   snprintf(ev, sizeof(ev), "tool_call.%s", phase ? phase : "");
   cJSON *f = cJSON_CreateObject();
   cJSON_AddStringToObject(f, "type", ev);
   cJSON_AddStringToObject(f, "name", name ? name : "");
   char *frame = cJSON_PrintUnformatted(f);
   if (frame)
   {
      openai_runs_store_append_event(run_id, ev, frame);
      free(frame);
   }
   cJSON_Delete(f);
}

static void *run_job_worker(void *arg)
{
   run_job_t *j = (run_job_t *)arg;
   char *buf = (char *)malloc(RUN_JSON_CAP);
   if (!buf)
   {
      free(j->prompt);
      free(j);
      return NULL;
   }

   /* Cancelled before we even start. */
   if (openai_runs_store_cancel_requested(j->run_id))
   {
      openai_runs_store_append_event(j->run_id, "response.completed",
                                     run_status_json(j, "cancelled", buf, RUN_JSON_CAP));
      openai_runs_store_finalize(j->run_id, OPENAI_RUN_CANCELLED,
                                 run_status_json(j, "cancelled", buf, RUN_JSON_CAP));
      goto done;
   }

   openai_runs_store_set_status(j->run_id, OPENAI_RUN_IN_PROGRESS);
   openai_runs_store_append_event(j->run_id, "response.in_progress",
                                  run_status_json(j, "in_progress", buf, RUN_JSON_CAP));

   agent_config_t acfg;
   agent_t *ag = stream_pick_agent(&acfg, j->model);
   if (!ag)
   {
      openai_runs_store_append_event(j->run_id, "error", "{\"error\":\"no agent configured\"}");
      openai_runs_store_finalize(j->run_id, OPENAI_RUN_FAILED,
                                 run_status_json(j, "failed", buf, RUN_JSON_CAP));
      goto done;
   }

   agent_result_t result;
   memset(&result, 0, sizeof(result));
   /* /v1/runs is the agentic endpoint (vs. the stateless /v1/chat/completions):
    * run tool-enabled so the run can use aimee's tools and emit tool_call events
    * (AC #7). agent_run_with_tools self-preps the provider catalog and drives the
    * tool loop; the tool-event hook fires from dispatch_tool_call_ctx within it.
    * (ag was validated above as a configured agent; agent_run_with_tools routes
    * by role within the same config — the model field is informational here.) */
   (void)ag;
   agent_tools_set_tool_event_cb(runs_tool_event_cb, (void *)j->run_id);
   int erc = agent_run_with_tools(&acfg, "execute", NULL, j->prompt, j->max_tokens, &result);
   agent_tools_set_tool_event_cb(NULL, NULL);

   /* Honor a cancel requested while the (blocking) step ran. */
   if (openai_runs_store_cancel_requested(j->run_id))
   {
      free(result.response);
      openai_runs_store_append_event(j->run_id, "response.completed",
                                     run_status_json(j, "cancelled", buf, RUN_JSON_CAP));
      openai_runs_store_finalize(j->run_id, OPENAI_RUN_CANCELLED,
                                 run_status_json(j, "cancelled", buf, RUN_JSON_CAP));
      goto done;
   }

   if (erc != 0 || !result.response)
   {
      char errbuf[256];
      snprintf(errbuf, sizeof(errbuf), "{\"error\":\"%s\"}",
               result.error[0] ? result.error : "run failed");
      openai_runs_store_append_event(j->run_id, "error", errbuf);
      openai_runs_store_finalize(j->run_id, OPENAI_RUN_FAILED,
                                 run_status_json(j, "failed", buf, RUN_JSON_CAP));
      free(result.response);
      goto done;
   }

   /* Success: emit the output as delta events, then a terminal completed frame. */
   {
      char item_id[160];
      snprintf(item_id, sizeof(item_id), "%s-msg", j->run_id);
      const char *txt = result.response;
      size_t n = strlen(txt);
      for (size_t off = 0; off < n; off += RUN_DELTA)
      {
         char seg[RUN_DELTA + 1];
         size_t take = (n - off < RUN_DELTA) ? (n - off) : RUN_DELTA;
         memcpy(seg, txt + off, take);
         seg[take] = '\0';
         char frame[RUN_DELTA * 6 + 256];
         if (openai_format_responses_delta(item_id, seg, frame, sizeof(frame)) > 0)
            openai_runs_store_append_event(j->run_id, "response.output_text.delta", frame);
      }
      /* message.completed: the full assistant message is now available (the AC's
       * terminal message event, distinct from the run-level response.completed). */
      cJSON *mc = cJSON_CreateObject();
      cJSON_AddStringToObject(mc, "type", "message.completed");
      cJSON_AddStringToObject(mc, "item_id", item_id);
      cJSON *msg = cJSON_AddObjectToObject(mc, "message");
      cJSON_AddStringToObject(msg, "role", "assistant");
      cJSON_AddStringToObject(msg, "content", txt);
      char *mframe = cJSON_PrintUnformatted(mc);
      if (mframe)
      {
         openai_runs_store_append_event(j->run_id, "message.completed", mframe);
         free(mframe);
      }
      cJSON_Delete(mc);
   }
   if (openai_format_run(j->run_id, j->model, result.response, j->created, result.prompt_tokens,
                         result.completion_tokens, "completed", buf, RUN_JSON_CAP) > 0)
   {
      openai_runs_store_append_event(j->run_id, "response.completed", buf);
      openai_runs_store_finalize(j->run_id, OPENAI_RUN_COMPLETED, buf);
   }
   else
   {
      openai_runs_store_finalize(j->run_id, OPENAI_RUN_FAILED,
                                 "{\"object\":\"run\",\"status\":\"failed\"}");
   }
   free(result.response);

done:
   free(buf);
   free(j->prompt);
   free(j);
   return NULL;
}

/* POST /v1/runs: parse the request, create a `queued` run, spawn the background
 * worker, and return the queued run object immediately (the listener thread is
 * never blocked on inference). GET /v1/runs/{id}/events streams the worker's
 * live events; POST /v1/runs/{id}/stop requests cancellation. */
static int runs_handler(const char *body, char *resp, int cap)
{
   char model[64] = "";
   char prev_id[128] = "";
   char *prompt = NULL;
   int stream = 0;
   if (openai_parse_responses_request(body, model, sizeof(model), &prompt, prev_id, sizeof(prev_id),
                                      &stream) != 0)
   {
      openai_format_error(resp, cap, "invalid_request_error",
                          "invalid run request: expected non-empty `input`");
      return 400;
   }

   /* Validate an agent is configured synchronously so callers still get 503 fast. */
   agent_config_t acfg;
   agent_t *ag = stream_pick_agent(&acfg, model);
   if (!ag)
   {
      free(prompt);
      openai_format_error(resp, cap, "server_error", "no agent configured");
      return 503;
   }

   double temperature = openai_request_double(body, "temperature", OPENAI_CHAT_TEMPERATURE, 2.0);
   int max_tokens = openai_request_int(body, "max_tokens", OPENAI_CHAT_MAX_TOKENS, 32768);

   long created = (long)time(NULL);
   static atomic_ulong g_run_seq = 0; /* connections run concurrently; atomic */
   char id[64];
   snprintf(id, sizeof(id), "run_%ld_%lu", created, atomic_fetch_add(&g_run_seq, 1) + 1);

   /* Queued snapshot returned to the caller and stored for GET /v1/runs/{id}. */
   int len = openai_format_run(id, model, "", created, 0, 0, "queued", resp, cap);
   if (len < 0)
   {
      free(prompt);
      openai_format_error(resp, cap, "server_error", "run did not fit the buffer");
      return 500;
   }
   openai_runs_store_create(id, resp);

   run_job_t *j = (run_job_t *)calloc(1, sizeof(*j));
   if (!j)
   {
      free(prompt);
      openai_runs_store_finalize(id, OPENAI_RUN_FAILED, resp);
      return 200; /* resp still holds the queued object */
   }
   snprintf(j->run_id, sizeof(j->run_id), "%s", id);
   snprintf(j->model, sizeof(j->model), "%s", model);
   j->prompt = prompt; /* ownership moves to the worker */
   j->temperature = temperature;
   j->max_tokens = max_tokens;
   j->created = created;

   pthread_t th;
   if (pthread_create(&th, NULL, run_job_worker, j) != 0)
   {
      free(j->prompt);
      free(j);
      openai_runs_store_finalize(id, OPENAI_RUN_FAILED, resp);
      return 200;
   }
   pthread_detach(th);
   return 200; /* resp already holds the queued run object */
}

void openai_chat_register(void)
{
   server_http_set_chat_handler(chat_completions_handler);
   server_http_set_runs_handler(runs_handler);
   server_http_set_chat_stream_handler(chat_stream_handler);
   server_http_set_completion_stream_handler(completion_stream_handler);
   server_http_set_responses_stream_handler(responses_stream_handler);
   server_http_set_completion_handler(completions_handler);
   server_http_set_embeddings_handler(embeddings_handler);
   server_http_set_responses_handler(responses_handler);
   server_http_set_models_provider(models_provider);
}
