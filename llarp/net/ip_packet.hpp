#ifndef LLARP_IP_HPP
#define LLARP_IP_HPP

#include <ev/ev.h>
#include <net/net.hpp>
#include <util/buffer.hpp>
#include <util/time.hpp>

#ifndef _WIN32
// unix, linux
#include <sys/types.h>  // FreeBSD needs this for uchar for ip.h
#include <netinet/in.h>
#include <netinet/ip.h>
// anything not win32
struct ip_header
{
#ifdef __LITTLE_ENDIAN__
  unsigned int ihl : 4;
  unsigned int version : 4;
#elif defined(__BIG_ENDIAN__)
  unsigned int version : 4;
  unsigned int ihl : 4;
#else
#error "Please fix <bits/endian.h>"
#endif

#if defined(__linux__)
#define ip_version version
#endif
  uint8_t tos;
  uint16_t tot_len;
  uint16_t id;
  uint16_t frag_off;
  uint8_t ttl;
  uint8_t protocol;
  uint16_t check;
  uint32_t saddr;
  uint32_t daddr;
};
#else
// windows nt
#include <winsock2.h>
typedef struct ip_hdr
{
  unsigned char ip_header_len : 4;  // 4-bit header length (in 32-bit words) normally=5
                                    // (Means 20 Bytes may be 24 also)
  unsigned char version : 4;        // 4-bit IPv4 version
  unsigned char ip_tos;             // IP type of service
  unsigned short ip_total_length;   // Total length
  unsigned short ip_id;             // Unique identifier

  unsigned char ip_frag_offset : 5;  // Fragment offset field

  unsigned char ip_more_fragment : 1;
  unsigned char ip_dont_fragment : 1;
  unsigned char ip_reserved_zero : 1;

  unsigned char ip_frag_offset1;  // fragment offset

  unsigned char ip_ttl;        // Time to live
  unsigned char ip_protocol;   // Protocol(TCP,UDP etc)
  unsigned short ip_checksum;  // IP checksum
  unsigned int ip_srcaddr;     // Source address
  unsigned int ip_destaddr;    // Source address
} IPV4_HDR;
#define ip_header IPV4_HDR
#define saddr ip_srcaddr
#define daddr ip_destaddr
#define check ip_checksum
#define ihl ip_header_len
#define protocol ip_protocol
#define frag_off ip_frag_offset
#define tos ip_tos
#define ttl ip_ttl
#define tot_len ip_total_length
#endif

struct ipv6_header_preamble
{
  unsigned char version : 4;
  unsigned char pad_small : 4;
  uint8_t pad[3];
};

struct ipv6_header
{
  union
  {
    ipv6_header_preamble preamble;
    uint32_t flowlabel;
  } preamble;

  uint16_t payload_len;
  uint8_t proto;
  uint8_t hoplimit;
  in6_addr srcaddr;
  in6_addr dstaddr;

  /// get 20 bit truncated flow label in network order
  llarp::nuint32_t
  FlowLabel() const;

  /// put 20 bit truncated flow label network order
  void
  FlowLabel(llarp::nuint32_t flowlabel);
};

#include <memory>
#include <service/protocol_type.hpp>
#include <utility>

struct llarp_ev_loop;

namespace llarp
{
  namespace net
  {
    /// an Packet
    struct IPPacket
    {
      static constexpr size_t MaxSize = 1500;
      llarp_time_t timestamp;
      size_t sz;
      byte_t buf[MaxSize];

      ManagedBuffer
      Buffer();

      ManagedBuffer
      ConstBuffer() const;

      bool
      Load(const llarp_buffer_t& buf);

      struct GetTime
      {
        llarp_time_t
        operator()(const IPPacket& pkt) const
        {
          return pkt.timestamp;
        }
      };

      struct PutTime
      {
        llarp_ev_loop_ptr loop;
        PutTime(llarp_ev_loop_ptr evloop) : loop(std::move(evloop))
        {}
        void
        operator()(IPPacket& pkt) const
        {
          pkt.timestamp = llarp_ev_loop_time_now_ms(loop);
        }
      };

      struct GetNow
      {
        llarp_ev_loop_ptr loop;
        GetNow(llarp_ev_loop_ptr evloop) : loop(std::move(evloop))
        {}
        llarp_time_t
        operator()() const
        {
          return llarp_ev_loop_time_now_ms(loop);
        }
      };

      struct CompareSize
      {
        bool
        operator()(const IPPacket& left, const IPPacket& right)
        {
          return left.sz < right.sz;
        }
      };

      struct CompareOrder
      {
        bool
        operator()(const IPPacket& left, const IPPacket& right)
        {
          return left.timestamp < right.timestamp;
        }
      };

      inline ip_header*
      Header()
      {
        return (ip_header*)&buf[0];
      }

      inline const ip_header*
      Header() const
      {
        return (ip_header*)&buf[0];
      }

      inline ipv6_header*
      HeaderV6()
      {
        return (ipv6_header*)&buf[0];
      }

      inline const ipv6_header*
      HeaderV6() const
      {
        return (ipv6_header*)&buf[0];
      }

      inline int
      Version() const
      {
        return Header()->version;
      }

      inline bool
      IsV4() const
      {
        return Version() == 4;
      }

      inline bool
      IsV6() const
      {
        return Version() == 6;
      }

      inline service::ProtocolType
      ServiceProtocol() const
      {
        if (IsV4())
          return service::eProtocolTrafficV4;
        if (IsV6())
          return service::eProtocolTrafficV6;

        return service::eProtocolControl;
      }

      huint128_t
      srcv6() const;

      huint128_t
      dstv6() const;

      huint32_t
      srcv4() const;

      huint32_t
      dstv4() const;

      huint128_t
      src4to6() const;

      huint128_t
      dst4to6() const;

      huint128_t
      src4to6Lan() const;

      huint128_t
      dst4to6Lan() const;

      void
      UpdateIPv4Address(nuint32_t src, nuint32_t dst);

      void
      UpdateIPv6Address(
          huint128_t src, huint128_t dst, std::optional<nuint32_t> flowlabel = std::nullopt);

      /// set addresses to zero and recacluate checksums
      void
      ZeroAddresses(std::optional<nuint32_t> flowlabel = std::nullopt);

      /// zero out source address
      void
      ZeroSourceAddress(std::optional<nuint32_t> flowlabel = std::nullopt);

      /// make an icmp unreachable reply packet based of this ip packet
      std::optional<IPPacket>
      MakeICMPUnreachable() const;
    };

    /// generate ip checksum
    uint16_t
    ipchksum(const byte_t* buf, size_t sz, uint32_t sum = 0);
  }  // namespace net
}  // namespace llarp

#endif
