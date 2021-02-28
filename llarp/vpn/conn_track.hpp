#pragma once

#include <util/decaying_hashset.hpp>
#include <net/net_int.hpp>

namespace llarp::net
{
  struct IPPacket;
}

namespace llarp::vpn
{
  class ConnectionTracker
  {
    struct FlowHash
    {
      size_t
      operator()(const std::pair<uint8_t, nuint16_t>& pair) const
      {
        const std::hash<uint8_t> protoHash{};
        const std::hash<nuint16_t> portHash{};
        return (protoHash(pair.first) << 7) ^ portHash(pair.second);
      }
    };

    util::DecayingHashSet<std::pair<uint8_t, nuint16_t>, FlowHash> m_ActiveFlows{5min};

   public:
    void
    Decay(llarp_time_t now);

    bool
    HasFlowFor(const net::IPPacket& pkt) const;

    void
    HandleIPPacket(const net::IPPacket& pkt);
  };
}  // namespace llarp::vpn
