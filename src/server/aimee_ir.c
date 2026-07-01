/* aimee_ir.c -- lifecycle + accessors for the canonical IR. See aimee_ir.h.
 * Pure: depends only on cJSON + libc, so it unit-tests standalone. */
#include "aimee_ir.h"

#include "cJSON.h"

#include <stdlib.h>
#include <string.h>

static void free_str(char *s)
{
   free(s);
}

void aimee_block_free_contents(aimee_block_t *b)
{
   if (!b)
      return;
   free_str(b->text);
   free_str(b->tool_id);
   free_str(b->tool_name);
   cJSON_Delete(b->tool_input);
   cJSON_Delete(b->tool_result);
   free_str(b->media_type);
   free_str(b->media_ref);
   free_str(b->cache_control);
   cJSON_Delete(b->raw);
   memset(b, 0, sizeof *b);
}

static void free_blocks(aimee_block_t *blocks, int n)
{
   if (!blocks)
      return;
   for (int i = 0; i < n; i++)
      aimee_block_free_contents(&blocks[i]);
   free(blocks);
}

static void free_message(aimee_message_t *m)
{
   if (!m)
      return;
   free_str(m->role);
   free_blocks(m->blocks, m->n_blocks);
   cJSON_Delete(m->raw);
}

void aimee_request_free(aimee_request_t *r)
{
   if (!r)
      return;
   free_str(r->model);
   free_blocks(r->system, r->n_system);
   if (r->messages)
   {
      for (int i = 0; i < r->n_messages; i++)
         free_message(&r->messages[i]);
      free(r->messages);
   }
   if (r->tools)
   {
      for (int i = 0; i < r->n_tools; i++)
      {
         free_str(r->tools[i].name);
         free_str(r->tools[i].description);
         cJSON_Delete(r->tools[i].schema);
         free_str(r->tools[i].cache_control);
         cJSON_Delete(r->tools[i].raw);
      }
      free(r->tools);
   }
   cJSON_Delete(r->tool_choice);
   if (r->stop_sequences)
   {
      for (int i = 0; i < r->n_stop; i++)
         free_str(r->stop_sequences[i]);
      free(r->stop_sequences);
   }
   cJSON_Delete(r->raw);
   memset(r, 0, sizeof *r);
}

void aimee_response_free(aimee_response_t *r)
{
   if (!r)
      return;
   free_str(r->id);
   free_str(r->model);
   free_str(r->role);
   free_blocks(r->content, r->n_content);
   free_str(r->raw_stop_reason);
   cJSON_Delete(r->raw);
   memset(r, 0, sizeof *r);
}

size_t aimee_ir_last_user_text(const aimee_request_t *r, char *buf, size_t n)
{
   if (buf && n)
      buf[0] = '\0';
   if (!r || !buf || !n)
      return 0;
   /* find the LAST user-role message */
   const aimee_message_t *last = NULL;
   for (int i = 0; i < r->n_messages; i++)
      if (r->messages[i].role && strcmp(r->messages[i].role, "user") == 0)
         last = &r->messages[i];
   if (!last)
      return 0;
   /* concat its TEXT blocks (shape-agnostic; no silent drop of the message -- a
    * message that is only tool_result yields empty, which is correct). */
   size_t used = 0;
   for (int i = 0; i < last->n_blocks && used + 1 < n; i++)
   {
      const aimee_block_t *b = &last->blocks[i];
      if (b->type != AIMEE_BLK_TEXT || !b->text)
         continue;
      size_t l = strlen(b->text);
      if (l > n - 1 - used)
         l = n - 1 - used;
      memcpy(buf + used, b->text, l);
      used += l;
   }
   buf[used] = '\0';
   return used;
}

const char *aimee_stop_reason_name(aimee_stop_reason_t s)
{
   switch (s)
   {
   case AIMEE_STOP_END_TURN:
      return "end_turn";
   case AIMEE_STOP_MAX_TOKENS:
      return "max_tokens";
   case AIMEE_STOP_TOOL_USE:
      return "tool_use";
   case AIMEE_STOP_STOP_SEQUENCE:
      return "stop_sequence";
   case AIMEE_STOP_CONTENT_FILTER:
      return "content_filter";
   case AIMEE_STOP_ERROR:
      return "error";
   case AIMEE_STOP_UNKNOWN:
   default:
      return "unknown";
   }
}

aimee_stop_reason_t aimee_stop_reason_parse(const char *name)
{
   if (!name)
      return AIMEE_STOP_UNKNOWN;
   if (strcmp(name, "end_turn") == 0)
      return AIMEE_STOP_END_TURN;
   if (strcmp(name, "max_tokens") == 0)
      return AIMEE_STOP_MAX_TOKENS;
   if (strcmp(name, "tool_use") == 0)
      return AIMEE_STOP_TOOL_USE;
   if (strcmp(name, "stop_sequence") == 0)
      return AIMEE_STOP_STOP_SEQUENCE;
   if (strcmp(name, "content_filter") == 0)
      return AIMEE_STOP_CONTENT_FILTER;
   if (strcmp(name, "error") == 0)
      return AIMEE_STOP_ERROR;
   return AIMEE_STOP_UNKNOWN;
}
