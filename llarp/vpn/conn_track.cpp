#include <vpn/conn_track.hpp>
#include <net/ip_packet.hpp>

namespace llarp::vpn
{
  static std::pair<uint8_t, nuint16_t>
  GetFlowFromPacket(const net::IPPacket& pkt, bool isInbound)
  {
    const auto hdr = pkt.Header();
    nuint16_t port{};
    if (isInbound)
      port = nuint16_t{*reinterpret_cast<const uint16_t*>(pkt.buf + (hdr->ihl * 4))};
    else
      port = nuint16_t{*reinterpret_cast<const uint16_t*>(pkt.buf + (hdr->ihl * 4) + 2)};
    return {hdr->protocol, port};
  }

  void
  ConnectionTracker::HandleIPPacket(const net::IPPacket& pkt)
  {
    m_ActiveFlows.Insert(GetFlowFromPacket(pkt, false));
  }

  bool
  ConnectionTracker::HasFlowFor(const net::IPPacket& pkt) const
  {
    const auto proto = pkt.Header()->protocol;
    if (proto == 6 or proto == 17)
      return m_ActiveFlows.Contains(GetFlowFromPacket(pkt, true));
    return false;
  }

  void
  ConnectionTracker::Decay(llarp_time_t now)
  {
    m_ActiveFlows.Decay(now);
  }
}  // namespace llarp::vpn
