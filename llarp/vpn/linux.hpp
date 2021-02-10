#pragma once

#include <ev/vpn.hpp>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <vpn/common.hpp>
#include <linux/if_tun.h>
#include <linux/rtnetlink.h>

#include <util/str.hpp>

namespace llarp::vpn
{
  enum class GatewayMode
  {
    eFirstHop,
    eLowerDefault,
    eUpperDefault
  };

  /* Helper structure for ip address data and attributes */
  struct InetAddr
  {
    unsigned char family;
    unsigned char bitlen;
    unsigned char data[sizeof(struct in6_addr)];

    InetAddr(std::string address, size_t bitlen)
    {
      const char* addr = address.c_str();

      if (strchr(addr, ':'))
      {
        family = AF_INET6;
        bitlen = 128;
      }
      else
      {
        family = AF_INET;
        bitlen = bitlen;
      }
      inet_pton(family, addr, data);
    }
  };

  struct NLSocket
  {
    NLSocket() : sock{::socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)}
    {
      if (sock == -1)
        throw std::runtime_error("failed to make netlink socket");
    }

    ~NLSocket()
    {
      if (sock != -1)
        close(sock);
    }

    const int sock;

    int
    route(
        int cmd, int flags, const InetAddr* dst, const InetAddr* gw, GatewayMode mode, int if_idx);
  };

#define NLMSG_TAIL(nmsg) ((struct rtattr*)(((intptr_t)(nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

  /* Add new data to rtattr */
  int
  rtattr_add(struct nlmsghdr* n, unsigned int maxlen, int type, const void* data, int alen)
  {
    int len = RTA_LENGTH(alen);
    struct rtattr* rta;

    if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > maxlen)
    {
      fprintf(stderr, "rtattr_add error: message exceeded bound of %d\n", maxlen);
      return -1;
    }

    rta = NLMSG_TAIL(n);
    rta->rta_type = type;
    rta->rta_len = len;

    if (alen)
    {
      memcpy(RTA_DATA(rta), data, alen);
    }

    n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);

    return 0;
  }

  int
  NLSocket::route(
      int cmd, int flags, const InetAddr* dst, const InetAddr* gw, GatewayMode mode, int if_idx)
  {
    struct
    {
      struct nlmsghdr n;
      struct rtmsg r;
      char buf[4096];
    } nl_request{};

    /* Initialize request structure */
    nl_request.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    nl_request.n.nlmsg_flags = NLM_F_REQUEST | flags;
    nl_request.n.nlmsg_type = cmd;
    nl_request.n.nlmsg_pid = getpid();
    nl_request.r.rtm_family = dst->family;
    nl_request.r.rtm_table = RT_TABLE_MAIN;
    if (if_idx)
    {
      nl_request.r.rtm_scope = RT_SCOPE_LINK;
    }
    else
    {
      nl_request.r.rtm_scope = RT_SCOPE_NOWHERE;
    }
    /* Set additional flags if NOT deleting route */
    if (cmd != RTM_DELROUTE)
    {
      nl_request.r.rtm_protocol = RTPROT_BOOT;
      nl_request.r.rtm_type = RTN_UNICAST;
    }

    nl_request.r.rtm_family = dst->family;
    nl_request.r.rtm_dst_len = dst->bitlen;

    /* Select scope, for simplicity we supports here only IPv6 and IPv4 */
    if (nl_request.r.rtm_family == AF_INET6)
    {
      nl_request.r.rtm_scope = RT_SCOPE_UNIVERSE;
    }
    else
    {
      nl_request.r.rtm_scope = RT_SCOPE_LINK;
    }

    /* Set gateway */
    if (gw->bitlen != 0)
    {
      rtattr_add(&nl_request.n, sizeof(nl_request), RTA_GATEWAY, &gw->data, gw->bitlen / 8);
    }
    nl_request.r.rtm_scope = 0;
    nl_request.r.rtm_family = gw->family;
    if (mode == GatewayMode::eFirstHop)
    {
      rtattr_add(
          &nl_request.n, sizeof(nl_request), /*RTA_NEWDST*/ RTA_DST, &dst->data, dst->bitlen / 8);
      /* Set interface */
      rtattr_add(&nl_request.n, sizeof(nl_request), RTA_OIF, &if_idx, sizeof(int));
    }
    if (mode == GatewayMode::eUpperDefault)
    {
      rtattr_add(
          &nl_request.n, sizeof(nl_request), /*RTA_NEWDST*/ RTA_DST, &dst->data, sizeof(uint32_t));
    }
    /* Send message to the netlink */
    return send(sock, &nl_request, sizeof(nl_request), 0);
  }

  struct in6_ifreq
  {
    in6_addr addr;
    uint32_t prefixlen;
    unsigned int ifindex;
  };

  class LinuxInterface : public NetworkInterface
  {
    const int m_fd;
    const InterfaceInfo m_Info;

   public:
    LinuxInterface(InterfaceInfo info)
        : NetworkInterface{}, m_fd{::open("/dev/net/tun", O_RDWR)}, m_Info{std::move(info)}

    {
      if (m_fd == -1)
        throw std::runtime_error("cannot open /dev/net/tun " + std::string{strerror(errno)});

      ifreq ifr{};
      in6_ifreq ifr6{};
      ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
      std::copy_n(
          m_Info.ifname.c_str(),
          std::min(m_Info.ifname.size(), sizeof(ifr.ifr_name)),
          ifr.ifr_name);
      if (::ioctl(m_fd, TUNSETIFF, &ifr) == -1)
        throw std::runtime_error("cannot set interface name: " + std::string{strerror(errno)});
      IOCTL control{AF_INET};

      control.ioctl(SIOCGIFFLAGS, &ifr);
      const int flags = ifr.ifr_flags;
      control.ioctl(SIOCGIFINDEX, &ifr);
      const int ifindex = ifr.ifr_ifindex;

      IOCTL control6{AF_INET6};
      for (const auto& ifaddr : m_Info.addrs)
      {
        if (ifaddr.fam == AF_INET)
        {
          ifr.ifr_addr.sa_family = AF_INET;
          const nuint32_t addr = ToNet(net::TruncateV6(ifaddr.range.addr));
          ((sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr = addr.n;
          control.ioctl(SIOCSIFADDR, &ifr);

          const nuint32_t mask = ToNet(net::TruncateV6(ifaddr.range.netmask_bits));
          ((sockaddr_in*)&ifr.ifr_netmask)->sin_addr.s_addr = mask.n;
          control.ioctl(SIOCSIFNETMASK, &ifr);
        }
        if (ifaddr.fam == AF_INET6)
        {
          ifr6.addr = net::HUIntToIn6(ifaddr.range.addr);
          ifr6.prefixlen = llarp::bits::count_bits(ifaddr.range.netmask_bits);
          ifr6.ifindex = ifindex;
          control6.ioctl(SIOCSIFADDR, &ifr6);
        }
      }
      ifr.ifr_flags = flags | IFF_UP | IFF_NO_PI;
      control.ioctl(SIOCSIFFLAGS, &ifr);
    }

    virtual ~LinuxInterface()
    {
      if (m_fd != -1)
        ::close(m_fd);
    }

    int
    PollFD() const override
    {
      return m_fd;
    }

    net::IPPacket
    ReadNextPacket() override
    {
      net::IPPacket pkt;
      const auto sz = read(m_fd, pkt.buf, sizeof(pkt.buf));
      if (sz >= 0)
        pkt.sz = std::min(sz, ssize_t{sizeof(pkt.buf)});
      return pkt;
    }

    bool
    WritePacket(net::IPPacket pkt) override
    {
      const auto sz = write(m_fd, pkt.buf, pkt.sz);
      if (sz <= 0)
        return false;
      return sz == static_cast<ssize_t>(pkt.sz);
    }

    bool
    HasNextPacket() override
    {
      return false;
    }

    InterfaceInfo
    GetInfo() const override
    {
      return m_Info;
    }
  };

  class LinuxPlatform : public Platform
  {
    NLSocket sock;

   public:
    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info) override
    {
      return std::make_shared<LinuxInterface>(std::move(info));
    };

    void AddRoute(RouteInfo<huint128_t>) override
    {}

    void DelRoute(RouteInfo<huint128_t>) override
    {}

    void
    AddRoute(RouteInfo<huint32_t> ip4) override
    {
      const InetAddr to_addr{ip4.addr.ToString(), bits::count_bits(ip4.netmask)};
      const InetAddr gw_addr{ip4.gateway.ToString(), bits::count_bits(ip4.netmask)};
      sock.route(
          RTM_NEWROUTE, NLM_F_CREATE | NLM_F_EXCL, &to_addr, &gw_addr, GatewayMode::eFirstHop, 0);
    }

    void
    DelRoute(RouteInfo<huint32_t> ip4) override
    {
      const InetAddr to_addr{ip4.addr.ToString(), bits::count_bits(ip4.netmask)};
      const InetAddr gw_addr{ip4.gateway.ToString(), bits::count_bits(ip4.netmask)};
      sock.route(RTM_DELROUTE, 0, &to_addr, &gw_addr, GatewayMode::eFirstHop, 0);
    }

    void
    AddDefaultRouteVia(std::shared_ptr<NetworkInterface> netif) override
    {
      if (not netif)
        return;
      const auto ifname = netif->IfName();
      int if_idx = if_nametoindex(ifname.c_str());
      const auto maybe = GetIFAddr(ifname);
      if (not maybe)
        throw std::runtime_error{"we dont have our own network interface"};

      const InetAddr to_addr_lower{"0.0.0.0", 1};
      const InetAddr to_addr_upper{"128.0.0.0", 1};
      const InetAddr gateway{maybe->toHost(), 32};

      sock.route(
          RTM_NEWROUTE,
          NLM_F_CREATE | NLM_F_EXCL,
          &to_addr_lower,
          &gateway,
          GatewayMode::eLowerDefault,
          if_idx);
      sock.route(
          RTM_NEWROUTE,
          NLM_F_CREATE | NLM_F_EXCL,
          &to_addr_upper,
          &gateway,
          GatewayMode::eUpperDefault,
          if_idx);
    }

    void
    DelDefaultRouteVia(std::shared_ptr<NetworkInterface> netif) override
    {
      if (not netif)
        return;
      const auto ifname = netif->IfName();
      int if_idx = if_nametoindex(ifname.c_str());
      const auto maybe = GetIFAddr(ifname);
      if (not maybe)
        throw std::runtime_error{"we dont have our own network interface"};

      const InetAddr to_addr_lower{"0.0.0.0", 1};
      const InetAddr to_addr_upper{"128.0.0.0", 1};
      const InetAddr gateway{maybe->toHost(), 32};

      sock.route(RTM_DELROUTE, 0, &to_addr_lower, &gateway, GatewayMode::eLowerDefault, if_idx);
      sock.route(RTM_DELROUTE, 0, &to_addr_upper, &gateway, GatewayMode::eUpperDefault, if_idx);
    }

    std::vector<huint32_t>
    GetDefaultGatewaysNotOn(std::shared_ptr<NetworkInterface> interface) override
    {
      std::string ifname;
      std::vector<huint32_t> gateways;
      if (interface)
        ifname = interface->IfName();
      std::ifstream inf("/proc/net/route");
      for (std::string line; std::getline(inf, line);)
      {
        const auto parts = split(line, '\t');
        if (parts[1].find_first_not_of('0') == std::string::npos and parts[0] != ifname)
        {
          const auto& ip = parts[2];
          if ((ip.size() == sizeof(uint32_t) * 2) and oxenmq::is_hex(ip))
          {
            huint32_t x{};
            oxenmq::from_hex(ip.begin(), ip.end(), reinterpret_cast<char*>(&x.h));
            gateways.emplace_back(x);
          }
        }
      }
      return gateways;
    }
  };

}  // namespace llarp::vpn
