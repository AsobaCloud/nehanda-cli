/* platform_registry.c: built-in gateway platform registry.
 * Registers the Phase-1 platform adapters (Telegram, ntfy, Webhook).
 */
#include "gateway_platform.h"
#include "platform_telegram.h"
#include "platform_ntfy.h"
#include "platform_webhook.h"

#include <string.h>

/* --- registry implementation --- */

#define MAX_ADAPTERS 16
static platform_adapter_t *s_registry[MAX_ADAPTERS];
static int s_registry_count = 0;

int gateway_platform_count(void)
{
   return s_registry_count;
}

platform_adapter_t *gateway_platform_get(int idx)
{
   if (idx < 0 || idx >= s_registry_count)
      return NULL;
   return s_registry[idx];
}

int gateway_platform_register(platform_adapter_t *adapter)
{
   if (!adapter || !adapter->name)
      return -1;
   for (int i = 0; i < s_registry_count; i++)
   {
      if (strcmp(s_registry[i]->name, adapter->name) == 0)
         return -1; /* already registered */
   }
   if (s_registry_count >= MAX_ADAPTERS)
      return -1;
   s_registry[s_registry_count++] = adapter;
   return 0;
}

int gateway_platform_unregister(const char *name)
{
   if (!name)
      return -1;
   for (int i = 0; i < s_registry_count; i++)
   {
      if (strcmp(s_registry[i]->name, name) == 0)
      {
         /* shift remaining entries down */
         for (int j = i; j < s_registry_count - 1; j++)
            s_registry[j] = s_registry[j + 1];
         s_registry_count--;
         return 0;
      }
   }
   return -1;
}

platform_adapter_t *gateway_platform_find(const char *name)
{
   if (!name)
      return NULL;
   for (int i = 0; i < s_registry_count; i++)
   {
      if (strcmp(s_registry[i]->name, name) == 0)
         return s_registry[i];
   }
   return NULL;
}

/* --- built-in entry list for --list-platforms and check_config --- */

static gateway_platform_entry_t s_entries[3] = {
    {"telegram", "Telegram", 0},
    {"ntfy", "ntfy", 0},
    {"webhook", "Webhook", 0},
};

static int s_initialised = 0;

static void ensure_init(void)
{
   if (s_initialised)
      return;
   s_initialised = 1;
   /* Register the real Phase-1 adapters. */
   gateway_platform_register(telegram_adapter_get());
   gateway_platform_register(ntfy_adapter_get());
   gateway_platform_register(webhook_adapter_get());
}

const gateway_platform_entry_t *gateway_platform_entries(int *count_out)
{
   ensure_init();
   if (count_out)
      *count_out = 3;
   return s_entries;
}
