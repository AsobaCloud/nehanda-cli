/* kb_background.c: aimee-kb autonomous-task slot tracker.
 * See kb_background.h for the contract. */
#include "kb_background.h"
#include "cJSON.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct
{
   int active;
   char name[KB_BACKGROUND_NAME_MAX];
   char descriptor[KB_BACKGROUND_DESCRIPTOR_MAX];
   time_t started_at;
} kb_bg_slot_t;

static kb_bg_slot_t g_slots[KB_BACKGROUND_SLOT_MAX];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static int find_slot_by_name(const char *name)
{
   for (int i = 0; i < KB_BACKGROUND_SLOT_MAX; i++)
   {
      if (g_slots[i].active && strncmp(g_slots[i].name, name, KB_BACKGROUND_NAME_MAX) == 0)
         return i;
   }
   return -1;
}

static int find_free_slot(void)
{
   for (int i = 0; i < KB_BACKGROUND_SLOT_MAX; i++)
   {
      if (!g_slots[i].active)
         return i;
   }
   return -1;
}

void kb_background_set(const char *name, const char *descriptor_fmt, ...)
{
   if (!name || !name[0])
      return;

   char buf[KB_BACKGROUND_DESCRIPTOR_MAX];
   buf[0] = '\0';
   if (descriptor_fmt && descriptor_fmt[0])
   {
      va_list ap;
      va_start(ap, descriptor_fmt);
      vsnprintf(buf, sizeof(buf), descriptor_fmt, ap);
      va_end(ap);
   }

   pthread_mutex_lock(&g_lock);
   int idx = find_slot_by_name(name);
   if (idx < 0)
      idx = find_free_slot();
   if (idx < 0)
   {
      pthread_mutex_unlock(&g_lock);
      return; /* registry full */
   }
   if (!g_slots[idx].active)
      g_slots[idx].started_at = time(NULL);
   g_slots[idx].active = 1;
   strncpy(g_slots[idx].name, name, sizeof(g_slots[idx].name) - 1);
   g_slots[idx].name[sizeof(g_slots[idx].name) - 1] = '\0';
   strncpy(g_slots[idx].descriptor, buf, sizeof(g_slots[idx].descriptor) - 1);
   g_slots[idx].descriptor[sizeof(g_slots[idx].descriptor) - 1] = '\0';
   pthread_mutex_unlock(&g_lock);
}

void kb_background_clear(const char *name)
{
   if (!name || !name[0])
      return;
   pthread_mutex_lock(&g_lock);
   int idx = find_slot_by_name(name);
   if (idx >= 0)
   {
      g_slots[idx].active = 0;
      g_slots[idx].name[0] = '\0';
      g_slots[idx].descriptor[0] = '\0';
      g_slots[idx].started_at = 0;
   }
   pthread_mutex_unlock(&g_lock);
}

struct cJSON *kb_workers_response_build(int configured, char *conn_slots_json)
{
   cJSON *resp = cJSON_CreateObject();
   if (!resp)
   {
      free(conn_slots_json);
      return NULL;
   }
   cJSON_AddStringToObject(resp, "status", "ok");
   cJSON_AddNumberToObject(resp, "configured", configured);
   if (conn_slots_json)
   {
      cJSON *slots = cJSON_Parse(conn_slots_json);
      free(conn_slots_json);
      if (slots)
         cJSON_AddItemToObject(resp, "slots", slots);
   }
   char *bg_json = kb_background_slots_json();
   if (bg_json)
   {
      cJSON *bg = cJSON_Parse(bg_json);
      free(bg_json);
      if (bg)
         cJSON_AddItemToObject(resp, "background", bg);
   }
   return resp;
}

char *kb_background_slots_json(void)
{
   cJSON *arr = cJSON_CreateArray();
   if (!arr)
      return NULL;
   pthread_mutex_lock(&g_lock);
   time_t now = time(NULL);
   for (int i = 0; i < KB_BACKGROUND_SLOT_MAX; i++)
   {
      if (!g_slots[i].active)
         continue;
      cJSON *o = cJSON_CreateObject();
      if (!o)
         continue;
      cJSON_AddStringToObject(o, "name", g_slots[i].name);
      cJSON_AddBoolToObject(o, "active", 1);
      cJSON_AddStringToObject(o, "descriptor", g_slots[i].descriptor);
      cJSON_AddNumberToObject(o, "elapsed_secs", (double)(now - g_slots[i].started_at));
      cJSON_AddItemToArray(arr, o);
   }
   pthread_mutex_unlock(&g_lock);
   char *out = cJSON_PrintUnformatted(arr);
   cJSON_Delete(arr);
   return out;
}
