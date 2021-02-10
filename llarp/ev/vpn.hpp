#pragma once

#include <net/ip_range.hpp>
#include <net/ip_packet.hpp>
#include <set>

namespace llarp
{
  struct Context;
}  // namespace llarp

namespace llarp::vpn
{
  struct InterfaceAddress
  {
    constexpr InterfaceAddress(IPRange r, int f = AF_INET) : range{std::move(r)}, fam{f}
    {}
    IPRange range;
    int fam;
    bool
    operator<(const InterfaceAddress& other) const
    {
      return range < other.range or fam < other.fam;
    }

    bool
    IsV4() const
    {
      return fam == AF_INET;
    }

    huint32_t
    ToV4() const
    {
      return net::TruncateV6(range.addr);
    }
  };

  struct InterfaceInfo
  {
    std::string ifname;
    huint32_t dnsaddr;
    std::set<InterfaceAddress> addrs;
  };

  /// a vpn network interface
  class NetworkInterface
  {
   public:
    NetworkInterface() = default;
    NetworkInterface(const NetworkInterface&) = delete;
    NetworkInterface(NetworkInterface&&) = delete;

    virtual ~NetworkInterface() = default;

    /// get pollable fd for reading
    virtual int
    PollFD() const = 0;

    /// the interface's name
    std::string
    IfName() const
    {
      return GetInfo().ifname;
    }

    virtual InterfaceInfo
    GetInfo() const = 0;

    /// read next ip packet
    /// blocks until ready
    virtual net::IPPacket
    ReadNextPacket() = 0;

    /// return true if we have another packet to read
    virtual bool
    HasNextPacket() = 0;

    /// write a packet to the interface
    /// returns false if we dropped it
    virtual bool
    WritePacket(net::IPPacket pkt) = 0;
  };

  /// an ip route spec
  template <typename IP_t>
  struct RouteInfo
  {
    /// gateway this route is reachable by
    IP_t gateway;
    /// address of remote route
    IP_t addr;
    /// netmask of remote route
    IP_t netmask;
  };

  /// a vpn platform
  /// responsible for obtaining vpn interfaces
  class Platform
  {
   public:
    Platform() = default;
    Platform(const Platform&) = delete;
    Platform(Platform&&) = delete;
    virtual ~Platform() = default;

    /// get a new network interface fully configured given the interface info
    /// blocks until ready, throws on error
    virtual std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info) = 0;

    /// add ip6 route
    virtual void
    AddRoute(RouteInfo<huint128_t> ip6route) = 0;

    /// add ip4 route
    virtual void
    AddRoute(RouteInfo<huint32_t> ip4route) = 0;

    /// delete ip6 route
    virtual void
    DelRoute(RouteInfo<huint128_t> ip6route) = 0;

    /// delete ip4 route
    virtual void
    DelRoute(RouteInfo<huint32_t> ip4route) = 0;

    /// set default route via this vpn interface
    virtual void
    AddDefaultRouteVia(std::shared_ptr<NetworkInterface> interface) = 0;

    /// remove default route via this vpn interface
    virtual void
    DelDefaultRouteVia(std::shared_ptr<NetworkInterface> interface) = 0;

    /// get default gateways that are not owned by this network interface
    virtual std::vector<huint32_t>
    GetDefaultGatewaysNotOn(std::shared_ptr<NetworkInterface> interface) = 0;
  };

  /// create native vpn platform
  std::shared_ptr<Platform>
  MakeNativePlatform(llarp::Context* ctx);

}  // namespace llarp::vpn
