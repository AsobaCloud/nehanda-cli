/* platform_event.c: epoll-based event loop (Linux) */
#include "platform_event.h"
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>

int platform_evloop_create(platform_evloop_t *loop)
{
   loop->epoll_fd = epoll_create1(0);
   return (loop->epoll_fd < 0) ? -1 : 0;
}

void platform_evloop_destroy(platform_evloop_t *loop)
{
   if (loop->epoll_fd >= 0)
   {
      close(loop->epoll_fd);
      loop->epoll_fd = -1;
   }
}

static uint32_t to_epoll(uint32_t flags)
{
   uint32_t ev = 0;
   if (flags & PLAT_EV_IN)
      ev |= EPOLLIN;
   if (flags & PLAT_EV_OUT)
      ev |= EPOLLOUT;
   return ev;
}

static uint32_t from_epoll(uint32_t ev)
{
   uint32_t flags = 0;
   if (ev & EPOLLIN)
      flags |= PLAT_EV_IN;
   if (ev & EPOLLOUT)
      flags |= PLAT_EV_OUT;
   if (ev & EPOLLERR)
      flags |= PLAT_EV_ERR;
   if (ev & EPOLLHUP)
      flags |= PLAT_EV_HUP;
   return flags;
}

int platform_evloop_add(platform_evloop_t *loop, int fd, uint32_t events)
{
   struct epoll_event ev = {.events = to_epoll(events), .data.fd = fd};
   return epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

int platform_evloop_mod(platform_evloop_t *loop, int fd, uint32_t events)
{
   struct epoll_event ev = {.events = to_epoll(events), .data.fd = fd};
   return epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

int platform_evloop_del(platform_evloop_t *loop, int fd)
{
   return epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
}

int platform_evloop_wait(platform_evloop_t *loop, platform_event_t *out, int max_events,
                         int timeout_ms)
{
   struct epoll_event raw[64];
   if (max_events > 64)
      max_events = 64;

   int n = epoll_wait(loop->epoll_fd, raw, max_events, timeout_ms);
   if (n < 0)
      return (errno == EINTR) ? 0 : -1;

   for (int i = 0; i < n; i++)
   {
      out[i].fd = raw[i].data.fd;
      out[i].events = from_epoll(raw[i].events);
   }
   return n;
}
