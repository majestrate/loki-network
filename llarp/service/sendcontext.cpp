#include "sendcontext.hpp"

#include <llarp/router/abstractrouter.hpp>
#include <llarp/routing/path_transfer_message.hpp>
#include "endpoint.hpp"
#include <utility>
#include <unordered_set>

namespace llarp
{
  namespace service
  {
    static constexpr size_t SendContextQueueSize = 512;

    SendContext::SendContext(
        ServiceInfo ident, const Introduction& intro, path::PathSet* send, Endpoint* ep)
        : remoteIdent(std::move(ident))
        , remoteIntro(intro)
        , m_PathSet(send)
        , m_DataHandler(ep)
        , m_Endpoint(ep)
        , createdAt(ep->Now())
        , m_SendQueue(SendContextQueueSize)
    {}

    bool
    SendContext::Send(std::shared_ptr<ProtocolFrame> msg, path::Path_ptr path)
    {
      if (m_SendQueue.empty() or m_SendQueue.full())
      {
        m_Endpoint->Loop()->call([this] { FlushUpstream(); });
      }
      m_SendQueue.pushBack(std::make_pair(
          std::make_shared<routing::PathTransferMessage>(*msg, remoteIntro.pathID), path));
      return true;
    }

    void
    SendContext::FlushUpstream()
    {
      auto r = m_Endpoint->Router();
      std::unordered_set<path::Path_ptr, path::Path::Ptr_Hash> flushpaths;
      auto rttRMS = 0ms;
      {
        do
        {
          auto maybe = m_SendQueue.tryPopFront();
          if (not maybe)
            break;
          auto& item = *maybe;
          item.first->S = item.second->NextSeqNo();
          if (item.second->SendRoutingMessage(*item.first, r))
          {
            lastGoodSend = r->Now();
            flushpaths.emplace(item.second);
            m_Endpoint->ConvoTagTX(item.first->T.T);
            const auto rtt = (item.second->intro.latency + remoteIntro.latency) * 2;
            rttRMS += rtt * rtt.count();
          }
        } while (not m_SendQueue.empty());
      }
      // flush the select path's upstream
      for (const auto& path : flushpaths)
      {
        path->FlushUpstream(r);
      }
      if (flushpaths.empty())
        return;
      estimatedRTT = std::chrono::milliseconds{
          static_cast<int64_t>(std::sqrt(rttRMS.count() / flushpaths.size()))};
    }

    /// send on an established convo tag
    void
    SendContext::EncryptAndSendTo(const llarp_buffer_t& payload, ProtocolType t)
    {
      SharedSecret shared;
      auto f = std::make_shared<ProtocolFrame>();
      f->R = 0;
      f->N.Randomize();
      f->T = currentConvoTag;
      f->S = ++sequenceNo;

      auto path = m_PathSet->GetPathByRouter(remoteIntro.router);
      if (!path)
      {
        LogWarn(m_Endpoint->Name(), " cannot encrypt and send: no path for intro ", remoteIntro);
        return;
      }

      if (!m_DataHandler->GetCachedSessionKeyFor(f->T, shared))
      {
        LogWarn(m_Endpoint->Name(), " has no cached session key on session T=", f->T);
        return;
      }

      auto m = std::make_shared<ProtocolMessage>();
      m_DataHandler->PutIntroFor(f->T, remoteIntro);
      m_DataHandler->PutReplyIntroFor(f->T, path->intro);
      m->proto = t;
      if (auto maybe = m_Endpoint->GetSeqNoForConvo(f->T))
      {
        m->seqno = *maybe;
      }
      else
      {
        LogWarn(m_Endpoint->Name(), " no session T=", f->T);
        return;
      }
      m->introReply = path->intro;
      f->F = m->introReply.pathID;
      m->sender = m_Endpoint->GetIdentity().pub;
      m->tag = f->T;
      m->PutBuffer(payload);
      m_Endpoint->Router()->QueueWork([f, m, shared, path, this] {
        if (not f->EncryptAndSign(*m, shared, m_Endpoint->GetIdentity()))
        {
          LogError(m_Endpoint->Name(), " failed to sign message");
          return;
        }
        Send(f, path);
      });
    }

    void
    SendContext::AsyncSendAuth(std::function<void(AuthResult)> resultHandler)
    {
      const auto maybe = m_Endpoint->MaybeGetAuthInfoForEndpoint(remoteIdent.Addr());
      if (maybe.has_value())
      {
        // send auth message
        const llarp_buffer_t authdata{maybe->token};
        AsyncGenIntro(authdata, ProtocolType::Auth);
        authResultListener = resultHandler;
      }
      else
        resultHandler({AuthResultCode::eAuthFailed, "no auth for given endpoint"});
    }

    void
    SendContext::AsyncEncryptAndSendTo(const llarp_buffer_t& data, ProtocolType protocol)
    {
      if (lastGoodSend != 0s)
      {
        EncryptAndSendTo(data, protocol);
        return;
      }
      const auto maybe = m_Endpoint->MaybeGetAuthInfoForEndpoint(remoteIdent.Addr());
      if (maybe.has_value())
      {
        // send auth message
        const llarp_buffer_t authdata(maybe->token);
        AsyncGenIntro(authdata, ProtocolType::Auth);
      }
      else
      {
        AsyncGenIntro(data, protocol);
      }
    }
  }  // namespace service

}  // namespace llarp
