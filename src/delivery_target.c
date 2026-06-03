/* delivery_target.c: typed outbound delivery target parsing. */
#include "delivery_target.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void target_clear(delivery_target_t *out)
{
   if (out)
      memset(out, 0, sizeof(*out));
}

static int target_copy(char *dst, size_t dst_len, const char *src, size_t src_len)
{
   if (!dst || dst_len == 0 || !src || src_len == 0 || src_len >= dst_len)
      return -1;
   memcpy(dst, src, src_len);
   dst[src_len] = '\0';
   return 0;
}

static int platform_valid(const char *s, size_t len)
{
   if (!s || len == 0 || len >= DELIVERY_TARGET_PLATFORM_MAX)
      return 0;
   for (size_t i = 0; i < len; i++)
   {
      unsigned char c = (unsigned char)s[i];
      if (!(islower(c) || isdigit(c) || c == '_' || c == '-'))
         return 0;
   }
   return 1;
}

static int component_valid(const char *s, size_t len, size_t cap)
{
   if (!s || len == 0 || len >= cap)
      return 0;
   for (size_t i = 0; i < len; i++)
   {
      unsigned char c = (unsigned char)s[i];
      if (iscntrl(c) || c == ':')
         return 0;
   }
   return 1;
}

int delivery_target_parse(const char *spec, delivery_target_t *out)
{
   target_clear(out);
   if (!spec || !spec[0] || !out)
      return -1;

   const char *first = strchr(spec, ':');
   if (!first)
   {
      size_t len = strlen(spec);
      if (!platform_valid(spec, len))
         return -1;
      return target_copy(out->platform, sizeof(out->platform), spec, len);
   }

   size_t platform_len = (size_t)(first - spec);
   if (!platform_valid(spec, platform_len))
      return -1;

   const char *chat = first + 1;
   const char *second = strchr(chat, ':');
   if (!second)
   {
      size_t chat_len = strlen(chat);
      if (!component_valid(chat, chat_len, sizeof(out->chat_id)))
         return -1;
      if (target_copy(out->platform, sizeof(out->platform), spec, platform_len) != 0 ||
          target_copy(out->chat_id, sizeof(out->chat_id), chat, chat_len) != 0)
         return -1;
      return 0;
   }

   if (strchr(second + 1, ':'))
      return -1;

   size_t chat_len = (size_t)(second - chat);
   size_t thread_len = strlen(second + 1);
   if (!component_valid(chat, chat_len, sizeof(out->chat_id)) ||
       !component_valid(second + 1, thread_len, sizeof(out->thread_id)))
      return -1;

   if (target_copy(out->platform, sizeof(out->platform), spec, platform_len) != 0 ||
       target_copy(out->chat_id, sizeof(out->chat_id), chat, chat_len) != 0 ||
       target_copy(out->thread_id, sizeof(out->thread_id), second + 1, thread_len) != 0)
      return -1;
   return 0;
}

int delivery_target_format(const delivery_target_t *target, char *buf, size_t bufsz)
{
   if (!target || !buf || bufsz == 0 || !target->platform[0])
      return -1;
   int n;
   if (target->thread_id[0])
      n = snprintf(buf, bufsz, "%s:%s:%s", target->platform, target->chat_id, target->thread_id);
   else if (target->chat_id[0])
      n = snprintf(buf, bufsz, "%s:%s", target->platform, target->chat_id);
   else
      n = snprintf(buf, bufsz, "%s", target->platform);
   return (n > 0 && (size_t)n < bufsz) ? 0 : -1;
}

int delivery_target_is_origin(const delivery_target_t *target)
{
   return target && strcmp(target->platform, "origin") == 0 && !target->chat_id[0] &&
          !target->thread_id[0];
}
