/* conversation.c: conversation branching and thread exploration */
#include "conversation.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Deep-copy a cJSON array (messages).  Returns a new array that the caller
 * must free, or NULL on failure. */
static cJSON *clone_messages(const cJSON *src)
{
   if (!src)
      return cJSON_CreateArray();
   return cJSON_Duplicate(src, 1);
}

void conv_threads_init(conv_threads_t *ct, cJSON *initial_messages)
{
   memset(ct, 0, sizeof(*ct));

   conv_thread_t *root = &ct->threads[0];
   root->id = 0;
   root->parent_id = -1;
   root->branch_msg_index = 0;
   root->label[0] = '\0';
   root->messages = initial_messages ? initial_messages : cJSON_CreateArray();

   ct->count = 1;
   ct->active = 0;
}

int conv_branch(conv_threads_t *ct, const char *label)
{
   if (!ct || ct->count >= CONV_MAX_THREADS)
      return -1;

   conv_thread_t *parent = &ct->threads[ct->active];
   int new_id = ct->count; /* simple incrementing id */
   int msg_count = parent->messages ? cJSON_GetArraySize(parent->messages) : 0;

   conv_thread_t *t = &ct->threads[ct->count];
   t->id = new_id;
   t->parent_id = parent->id;
   t->branch_msg_index = msg_count;

   if (label && label[0])
      snprintf(t->label, sizeof(t->label), "%s", label);
   else
      snprintf(t->label, sizeof(t->label), "thread-%d", new_id);

   /* Copy the parent's messages up to the current point. */
   t->messages = clone_messages(parent->messages);

   ct->count++;
   /* Switch to the new thread automatically */
   ct->active = ct->count - 1;

   return new_id;
}

int conv_switch(conv_threads_t *ct, int thread_id)
{
   if (!ct)
      return -1;
   for (int i = 0; i < ct->count; i++)
   {
      if (ct->threads[i].id == thread_id)
      {
         ct->active = i;
         return 0;
      }
   }
   return -1;
}

cJSON *conv_active_messages(const conv_threads_t *ct)
{
   if (!ct || ct->count == 0)
      return NULL;
   return ct->threads[ct->active].messages;
}

int conv_active_id(const conv_threads_t *ct)
{
   if (!ct || ct->count == 0)
      return -1;
   return ct->threads[ct->active].id;
}

void conv_threads_free(conv_threads_t *ct)
{
   if (!ct)
      return;
   for (int i = 0; i < ct->count; i++)
   {
      if (ct->threads[i].messages)
      {
         cJSON_Delete(ct->threads[i].messages);
         ct->threads[i].messages = NULL;
      }
   }
   ct->count = 0;
   ct->active = 0;
}
