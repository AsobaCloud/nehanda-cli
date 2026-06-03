/* platform_event.c: WSAPoll-based event loop (Windows) */
#include "platform_event.h"
#include <stdlib.h>

static short to_wsapoll(uint32_t flags)
{
   short ev = 0;
   if (flags & PLAT_EV_IN)
      ev |= POLLRDNORM;
   if (flags & PLAT_EV_OUT)
      ev |= POLLWRNORM;
   return ev;
}

static uint32_t from_wsapoll(short revents)
{
   uint32_t flags = 0;
   if (revents & (POLLRDNORM | POLLRDBAND | POLLIN))
      flags |= PLAT_EV_IN;
   if (revents & (POLLWRNORM | POLLOUT))
      flags |= PLAT_EV_OUT;
   if (revents & POLLERR)
      flags |= PLAT_EV_ERR;
   if (revents & POLLHUP)
      flags |= PLAT_EV_HUP;
   return flags;
}

int platform_evloop_create(platform_evloop_t *loop)
{
   WSADATA wsa;
   if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
      return -1;
   loop->poll_set = NULL;
   loop->poll_cap = 0;
   loop->poll_count = 0;
   return 0;
}

void platform_evloop_destroy(platform_evloop_t *loop)
{
   free(loop->poll_set);
   loop->poll_set = NULL;
   loop->poll_cap = 0;
   loop->poll_count = 0;
   WSACleanup();
}

int platform_evloop_add(platform_evloop_t *loop, int fd, uint32_t events)
{
   WSAPOLLFD *set = (WSAPOLLFD *)loop->poll_set;
   for (int i = 0; i < loop->poll_count; i++)
   {
      if ((int)set[i].fd == fd)
         return -1;
   }

   if (loop->poll_count == loop->poll_cap)
   {
      int new_cap = (loop->poll_cap > 0) ? loop->poll_cap * 2 : 8;
      WSAPOLLFD *tmp = realloc(loop->poll_set, sizeof(WSAPOLLFD) * (size_t)new_cap);
      if (!tmp)
         return -1;
      loop->poll_set = tmp;
      loop->poll_cap = new_cap;
      set = tmp;
   }

   set[loop->poll_count].fd = (SOCKET)fd;
   set[loop->poll_count].events = to_wsapoll(events);
   set[loop->poll_count].revents = 0;
   loop->poll_count++;
   return 0;
}

int platform_evloop_mod(platform_evloop_t *loop, int fd, uint32_t events)
{
   WSAPOLLFD *set = (WSAPOLLFD *)loop->poll_set;
   for (int i = 0; i < loop->poll_count; i++)
   {
      if ((int)set[i].fd == fd)
      {
         set[i].events = to_wsapoll(events);
         set[i].revents = 0;
         return 0;
      }
   }
   return -1;
}

int platform_evloop_del(platform_evloop_t *loop, int fd)
{
   WSAPOLLFD *set = (WSAPOLLFD *)loop->poll_set;
   for (int i = 0; i < loop->poll_count; i++)
   {
      if ((int)set[i].fd == fd)
      {
         if (i != loop->poll_count - 1)
            set[i] = set[loop->poll_count - 1];
         loop->poll_count--;
         return 0;
      }
   }
   return -1;
}

int platform_evloop_wait(platform_evloop_t *loop, platform_event_t *out, int max_events,
                         int timeout_ms)
{
   if (loop->poll_count == 0)
   {
      if (timeout_ms > 0)
         Sleep((DWORD)timeout_ms);
      return 0;
   }

   int rc = WSAPoll((WSAPOLLFD *)loop->poll_set, loop->poll_count, timeout_ms);
   if (rc == SOCKET_ERROR)
   {
      int err = WSAGetLastError();
      return (err == WSAEINTR) ? 0 : -1;
   }

   int count = 0;
   WSAPOLLFD *set = (WSAPOLLFD *)loop->poll_set;
   for (int i = 0; i < loop->poll_count && count < max_events; i++)
   {
      if (set[i].revents == 0)
         continue;
      out[count].fd = (int)set[i].fd;
      out[count].events = from_wsapoll(set[i].revents);
      count++;
      set[i].revents = 0;
   }
   return count;
}
