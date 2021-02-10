#ifndef LLARP_NET_NET_IF_HPP
#define LLARP_NET_NET_IF_HPP
#ifndef _WIN32
// this file is a shim include for #include <net/if.h>
#if defined(__linux__)
#include <linux/if.h>
extern "C" unsigned int
#ifndef __GLIBC__
if_nametoindex(const char* __ifname);
#else
if_nametoindex(const char* __ifname) __THROW;
#endif
#else
#include <net/if.h>
#endif
#endif

#include <ev/vpn.hpp>

namespace llarp::net
{
  /// get the name of the loopback interface
  std::string
  LoopbackInterfaceName();

  struct NetIF
  {
    std::vector<vpn::RouteInfo<huint32_t>> ip4addrs;
    std::vector<vpn::RouteInfo<huint128_t>> ip6addrs;
    std::string ifname;

    static std::vector<NetIF>
    FetchAll();
  };

}  // namespace llarp::net

#endif
