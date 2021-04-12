#pragma once

#include "exit_messages.hpp"
#include "service/protocol_type.hpp"
#include <llarp/net/ip_packet.hpp>
#include <llarp/path/pathbuilder.hpp>
#include <llarp/routing/transfer_traffic_message.hpp>
#include <llarp/constants/path.hpp>

#include <deque>
#include <queue>

namespace llarp
{
  class EndpointBase;

  namespace quic
  {
    class TunnelManager;
  }

  namespace exit
  {
    struct BaseSession;

    using BaseSession_ptr = std::shared_ptr<BaseSession>;

    using SessionReadyFunc = std::function<void(BaseSession_ptr)>;

    static constexpr auto LifeSpan = path::default_lifetime;

    /// a persisting exit session with an exit router
    struct BaseSession : public llarp::path::Builder,
                         public std::enable_shared_from_this<BaseSession>
    {
      static constexpr size_t MaxUpstreamQueueLength = 256;

      BaseSession(
          const llarp::RouterID& exitRouter,
          std::function<bool(const llarp_buffer_t&)> writepkt,
          AbstractRouter* r,
          size_t numpaths,
          size_t hoplen,
          EndpointBase* parent);

      ~BaseSession() override;

      std::shared_ptr<path::PathSet>
      GetSelf() override
      {
        return shared_from_this();
      }

      void
      BlacklistSNode(const RouterID snode) override;

      util::StatusObject
      ExtractStatus() const;

      bool
      ShouldBundleRC() const override
      {
        return m_BundleRC;
      }

      void
      ResetInternalState() override;

      bool UrgentBuild(llarp_time_t) const override;

      void
      HandlePathDied(llarp::path::Path_ptr p) override;

      bool
      CheckPathDead(path::Path_ptr p, llarp_time_t dlt);

      std::optional<std::vector<RouterContact>>
      GetHopsForBuild() override;

      bool
      ShouldBuildMore(llarp_time_t now) const override;

      void
      HandlePathBuilt(llarp::path::Path_ptr p) override;

      bool
      QueueUpstreamTraffic(
          llarp::net::IPPacket pkt, const size_t packSize, service::ProtocolType t);

      /// flush upstream to exit via paths
      bool
      FlushUpstream();

      /// flush downstream to user via tun
      void
      FlushDownstream();

      path::PathRole
      GetRoles() const override
      {
        return path::ePathRoleExit;
      }

      /// send close and stop session
      bool
      Stop() override;

      bool
      IsReady() const;

      const llarp::RouterID
      Endpoint() const
      {
        return m_ExitRouter;
      }

      std::optional<PathID_t>
      CurrentPath() const
      {
        if (m_CurrentPath.IsZero())
          return std::nullopt;
        return m_CurrentPath;
      }

      bool
      IsExpired(llarp_time_t now) const;

      bool
      LoadIdentityFromFile(const char* fname);

      void
      AddReadyHook(SessionReadyFunc func);

     protected:
      llarp::RouterID m_ExitRouter;
      llarp::SecretKey m_ExitIdentity;
      std::function<bool(const llarp_buffer_t&)> m_WritePacket;

      virtual void
      PopulateRequest(llarp::routing::ObtainExitMessage& msg) const = 0;

      bool
      HandleTrafficDrop(llarp::path::Path_ptr p, const llarp::PathID_t& path, uint64_t s);

      bool
      HandleGotExit(llarp::path::Path_ptr p, llarp_time_t b);

      bool
      HandleTraffic(
          llarp::path::Path_ptr p,
          const llarp_buffer_t& buf,
          uint64_t seqno,
          service::ProtocolType t);

     private:
      std::set<RouterID> m_SnodeBlacklist;

      using UpstreamTrafficQueue_t = std::deque<llarp::routing::TransferTrafficMessage>;
      using TieredQueue_t = std::map<uint8_t, UpstreamTrafficQueue_t>;
      TieredQueue_t m_Upstream;

      PathID_t m_CurrentPath;

      using DownstreamPkt = std::pair<uint64_t, llarp::net::IPPacket>;

      struct DownstreamPktSorter
      {
        bool
        operator()(const DownstreamPkt& left, const DownstreamPkt& right) const
        {
          return left.first < right.first;
        }
      };

      using DownstreamTrafficQueue_t =
          std::priority_queue<DownstreamPkt, std::vector<DownstreamPkt>, DownstreamPktSorter>;
      DownstreamTrafficQueue_t m_Downstream;

      uint64_t m_Counter;
      llarp_time_t m_LastUse;

      std::vector<SessionReadyFunc> m_PendingCallbacks;
      const bool m_BundleRC;
      EndpointBase* const m_Parent;

      void
      CallPendingCallbacks(bool success);
    };

    struct ExitSession final : public BaseSession
    {
      ExitSession(
          const llarp::RouterID& snodeRouter,
          std::function<bool(const llarp_buffer_t&)> writepkt,
          AbstractRouter* r,
          size_t numpaths,
          size_t hoplen,
          EndpointBase* parent)
          : BaseSession{snodeRouter, writepkt, r, numpaths, hoplen, parent}
      {}

      ~ExitSession() override = default;

      std::string
      Name() const override;

      virtual void
      SendPacketToRemote(const llarp_buffer_t& pkt, service::ProtocolType t) override;

     protected:
      void
      PopulateRequest(llarp::routing::ObtainExitMessage& msg) const override
      {
        // TODO: set expiration time
        msg.X = 0;
        msg.E = 1;
      }
    };

    struct SNodeSession final : public BaseSession
    {
      SNodeSession(
          const llarp::RouterID& snodeRouter,
          std::function<bool(const llarp_buffer_t&)> writepkt,
          AbstractRouter* r,
          size_t numpaths,
          size_t hoplen,
          bool useRouterSNodeKey,
          EndpointBase* parent);

      ~SNodeSession() override = default;

      std::string
      Name() const override;

      virtual void
      SendPacketToRemote(const llarp_buffer_t& pkt, service::ProtocolType t) override;

     protected:
      void
      PopulateRequest(llarp::routing::ObtainExitMessage& msg) const override
      {
        // TODO: set expiration time
        msg.X = 0;
        msg.E = 0;
      }
    };

  }  // namespace exit
}  // namespace llarp
