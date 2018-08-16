#ifndef EV_EPOLL_HPP
#define EV_EPOLL_HPP
#include <llarp/buffer.h>
#include <llarp/net.h>
#include <signal.h>
#include <sys/epoll.h>
#include <tuntap.h>
#include <unistd.h>
#include <cstdio>
#include "ev.hpp"
#include "llarp/net.hpp"
#include "logger.hpp"

namespace llarp
{
  struct udp_listener : public ev_io
  {
    llarp_udp_io* udp;

    udp_listener(int fd, llarp_udp_io* u) : ev_io(fd), udp(u){};

    ~udp_listener()
    {
    }

    virtual int
    read(void* buf, size_t sz)
    {
      sockaddr_in6 src;
      socklen_t slen = sizeof(sockaddr_in6);
      sockaddr* addr = (sockaddr*)&src;
      int ret        = ::recvfrom(fd, buf, sz, 0, addr, &slen);
      if(ret == -1)
        return -1;
      udp->recvfrom(udp, addr, buf, ret);
      return 0;
    }

    virtual int
    sendto(const sockaddr* to, const void* data, size_t sz)
    {
      socklen_t slen;
      switch(to->sa_family)
      {
        case AF_INET:
          slen = sizeof(struct sockaddr_in);
          break;
        case AF_INET6:
          slen = sizeof(struct sockaddr_in6);
          break;
        default:
          return -1;
      }
      ssize_t sent = ::sendto(fd, data, sz, SOCK_NONBLOCK, to, slen);
      if(sent == -1)
      {
        llarp::LogWarn(strerror(errno));
      }
      return sent;
    }
  };

  struct tun : public ev_io
  {
    llarp_tun_io* t;
    device* tunif;
    tun(llarp_tun_io* tio)
        : ev_io(-1)
        , t(tio)
        , tunif(tuntap_init())

              {

              };

    int
    sendto(const sockaddr* to, const void* data, size_t sz)
    {
      // TODO: implement me
      return -1;
    }

    int
    read(void* buf, size_t sz)
    {
      return tuntap_read(tunif, buf, sz);
    }

    bool
    setup()
    {
      if(tuntap_start(tunif, TUNTAP_MODE_TUNNEL, TUNTAP_ID_ANY) == -1)
        return false;
      if(tuntap_set_ifname(tunif, t->ifname) == -1)
        return false;
      if(tuntap_set_ip(tunif, t->ifaddr, t->netmask) == -1)
        return false;
      fd = tunif->tun_fd;
      return false;
    }

    ~tun()
    {
    }
  };
};  // namespace llarp

struct llarp_epoll_loop : public llarp_ev_loop
{
  int epollfd;
  int pipefds[2];
  llarp_epoll_loop() : epollfd(-1)
  {
    pipefds[0] = -1;
    pipefds[1] = -1;
  }

  ~llarp_epoll_loop()
  {
    if(pipefds[0] != -1)
      close(pipefds[0]);

    if(pipefds[1] != -1)
      close(pipefds[1]);

    if(epollfd != -1)
      close(epollfd);
  }

  bool
  running() const
  {
    return epollfd != -1;
  }

  bool
  init()
  {
    if(epollfd == -1)
      epollfd = epoll_create(1);
    if(epollfd != -1)
    {
      if(pipe(pipefds) == -1)
        return false;
      epoll_event sig_ev;

      sig_ev.data.fd = pipefds[0];
      sig_ev.events  = EPOLLIN;
      return epoll_ctl(epollfd, EPOLL_CTL_ADD, pipefds[0], &sig_ev) != -1;
    }
    return false;
  }

  int
  tick(int ms)
  {
    epoll_event events[1024];
    int result;

    result = epoll_wait(epollfd, events, 1024, ms);
    if(result > 0)
    {
      int idx = 0;
      while(idx < result)
      {
        // handle signalfd
        if(events[idx].data.fd == pipefds[0])
        {
          llarp::LogDebug("exiting epoll loop");
          return 0;
        }
        llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].data.ptr);
        if(events[idx].events & EPOLLIN)
        {
          ev->read(readbuf, sizeof(readbuf));
        }
        if(events[idx].events & EPOLLOUT)
        {
          ev->flush_write();
        }
        ++idx;
      }
    }
    tick_listeners();
    return result;
  }

  int
  run()
  {
    epoll_event events[1024];
    int result;
    do
    {
      result = epoll_wait(epollfd, events, 1024, EV_TICK_INTERVAL);
      if(result > 0)
      {
        int idx = 0;
        while(idx < result)
        {
          // handle signalfd
          if(events[idx].data.fd == pipefds[0])
          {
            llarp::LogDebug("exiting epoll loop");
            return 0;
          }
          llarp::ev_io* ev = static_cast< llarp::ev_io* >(events[idx].data.ptr);
          if(events[idx].events & EPOLLIN)
          {
            ev->read(readbuf, sizeof(readbuf));
          }
          if(events[idx].events & EPOLLOUT)
          {
            ev->flush_write();
          }
          ++idx;
        }
      }
      tick_listeners();
    } while(epollfd != -1);
    return result;
  }

  int
  udp_bind(const sockaddr* addr)
  {
    socklen_t slen;
    switch(addr->sa_family)
    {
      case AF_INET:
        slen = sizeof(struct sockaddr_in);
        break;
      case AF_INET6:
        slen = sizeof(struct sockaddr_in6);
        break;
      default:
        return -1;
    }
    int fd = socket(addr->sa_family, SOCK_DGRAM, 0);
    if(fd == -1)
    {
      perror("socket()");
      return -1;
    }

    if(addr->sa_family == AF_INET6)
    {
      // enable dual stack explicitly
      int dual = 1;
      if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &dual, sizeof(dual)) == -1)
      {
        // failed
        perror("setsockopt()");
        close(fd);
        return -1;
      }
    }
    llarp::Addr a(*addr);
    llarp::LogDebug("bind to ", a);
    if(bind(fd, addr, slen) == -1)
    {
      perror("bind()");
      close(fd);
      return -1;
    }

    return fd;
  }

  bool
  close_ev(llarp::ev_io* ev)
  {
    return epoll_ctl(epollfd, EPOLL_CTL_DEL, ev->fd, nullptr) != -1;
  }

  llarp::ev_io*
  create_tun(llarp_tun_io* tun)
  {
    llarp::tun* t = new llarp::tun(tun);
    if(t->setup())
      return t;
    delete t;
    return nullptr;
  }

  llarp::ev_io*
  create_udp(llarp_udp_io* l, const sockaddr* src)
  {
    int fd = udp_bind(src);
    if(fd == -1)
      return nullptr;
    llarp::udp_listener* listener = new llarp::udp_listener(fd, l);
    l->impl                       = listener;
    udp_listeners.push_back(l);
    return listener;
  }

  bool
  add_ev(llarp::ev_io* e)
  {
    epoll_event ev;
    ev.data.ptr = e;
    ev.events   = EPOLLIN | EPOLLOUT;
    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, e->fd, &ev) == -1)
    {
      delete e;
      return false;
    }
    return true;
  }

  bool
  udp_close(llarp_udp_io* l)
  {
    bool ret = false;
    llarp::udp_listener* listener =
        static_cast< llarp::udp_listener* >(l->impl);
    if(listener)
    {
      close_ev(listener);
      l->impl = nullptr;
      delete listener;
      udp_listeners.remove(l);
    }
    return ret;
  }

  void
  stop()
  {
    int i    = 1;
    auto val = write(pipefds[1], &i, sizeof(i));
    (void)val;
  }

  byte_t readbuf[2048];
};

#endif
