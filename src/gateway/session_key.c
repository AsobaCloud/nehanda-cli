/* session_key.c: canonical gateway session keys. */
#include "gateway_platform.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int key_component_valid(const char *s)
{
   if (!s || !s[0])
      return 0;
   for (const unsigned char *p = (const unsigned char *)s; *p; p++)
   {
      if (iscntrl(*p) || *p == ':')
         return 0;
   }
   return 1;
}

static int key_component_present_and_invalid(const char *s)
{
   return s && s[0] && !key_component_valid(s);
}

static const char *chat_type_or_default(const session_source_t *source)
{
   return key_component_valid(source->chat_type) ? source->chat_type : "unknown";
}

static int write_key(char *buf, size_t bufsz, const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   int n = vsnprintf(buf, bufsz, fmt, ap);
   va_end(ap);
   return (n > 0 && (size_t)n < bufsz) ? 0 : -1;
}

int gateway_session_key_build(const session_source_t *source, int group_sessions_per_user,
                              char *buf, size_t bufsz)
{
   if (!source || !buf || bufsz == 0 || !key_component_valid(source->platform))
      return -1;
   buf[0] = '\0';

   if (key_component_present_and_invalid(source->chat_type) ||
       key_component_present_and_invalid(source->chat_id) ||
       key_component_present_and_invalid(source->user_id) ||
       key_component_present_and_invalid(source->thread_id))
      return -1;

   int has_chat = key_component_valid(source->chat_id);
   int has_thread = key_component_valid(source->thread_id);
   int has_user = key_component_valid(source->user_id);
   const char *type = chat_type_or_default(source);

   if ((strcmp(type, "dm") == 0 || strcmp(type, "private") == 0) && has_chat)
   {
      if (has_thread)
         return write_key(buf, bufsz, "%s:dm:%s:%s", source->platform, source->chat_id,
                          source->thread_id);
      return write_key(buf, bufsz, "%s:dm:%s", source->platform, source->chat_id);
   }

   if (has_chat && has_thread)
      return write_key(buf, bufsz, "%s:thread:%s:%s", source->platform, source->chat_id,
                       source->thread_id);

   if (has_chat)
   {
      if (group_sessions_per_user && has_user)
         return write_key(buf, bufsz, "%s:%s:%s:%s", source->platform, type, source->chat_id,
                          source->user_id);
      return write_key(buf, bufsz, "%s:%s:%s", source->platform, type, source->chat_id);
   }

   return write_key(buf, bufsz, "%s:%s", source->platform, type);
}
