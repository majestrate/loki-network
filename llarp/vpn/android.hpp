#pragma once

#include <stdio.h>
#include <unistd.h>

#include <ev/vpn.hpp>
#include <vpn/common.hpp>
#include <llarp.hpp>

namespace llarp::vpn
{
  class AndroidInterface : public NetworkInterface
  {
    const int m_fd;
    const InterfaceInfo m_Info;  // likely 100% ignored on android, at least for now

   public:
    AndroidInterface(InterfaceInfo info, int fd) : m_fd(fd), m_Info(info)
    {
      if (m_fd == -1)
        throw std::runtime_error(
            "Error opening AndroidVPN layer FD: " + std::string{strerror(errno)});
    }

    virtual ~AndroidInterface()
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

  class AndroidPlatform : public Platform
  {
    const int fd;

   public:
    AndroidPlatform(llarp::Context* ctx) : fd(ctx->GetAndroidFD())
    {}

    std::shared_ptr<NetworkInterface>
    ObtainInterface(InterfaceInfo info) override
    {
      return std::make_shared<AndroidInterface>(std::move(info), fd);
    }

    /// add ip6 route
    void AddRoute(RouteInfo<huint128_t>) override{};

    /// add ip4 route
    void AddRoute(RouteInfo<huint32_t>) override{};

    /// delete ip6 route
    void DelRoute(RouteInfo<huint128_t>) override{};

    /// delete ip4 route
    void DelRoute(RouteInfo<huint32_t>) override{};

    /// set default route via this vpn interface
    void AddDefaultRouteVia(std::shared_ptr<NetworkInterface>) override{};

    /// remove default route via this vpn interface
    void DelDefaultRouteVia(std::shared_ptr<NetworkInterface>) override{};

    /// get default gateways that are not owned by this network interface
    std::vector<huint32_t> GetDefaultGatewaysNotOn(std::shared_ptr<NetworkInterface>) override
    {
      return {};
    };
  };

}  // namespace llarp::vpn
