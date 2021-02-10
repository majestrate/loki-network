#include <router/route_poker.hpp>
#include <router/abstractrouter.hpp>
#include <net/route.hpp>
#include <service/context.hpp>
#include <unordered_set>

namespace llarp
{
  void
  RoutePoker::AddRoute(huint32_t ip)
  {
    m_PokedRoutes.emplace(ip, m_CurrentGateway);
    if (m_CurrentGateway.h == 0)
    {
      llarp::LogDebug("RoutePoker::AddRoute no current gateway, cannot enable route.");
    }
    else if (m_Enabled or m_Enabling)
    {
      llarp::LogInfo(
          "RoutePoker::AddRoute enabled, enabling route to ", ip, " via ", m_CurrentGateway);
      EnableRoute(ip, m_CurrentGateway);
    }
    else
    {
      llarp::LogDebug("RoutePoker::AddRoute disabled, not enabling route.");
    }
  }

  void
  RoutePoker::DisableRoute(huint32_t ip, huint32_t gateway)
  {
    m_Router->GetVPNPlatform()->DelRoute(
        vpn::RouteInfo<huint32_t>{gateway, ip, netmask_ipv4_bits(32)});
  }

  void
  RoutePoker::EnableRoute(huint32_t ip, huint32_t gateway)
  {
    m_Router->GetVPNPlatform()->AddRoute(
        vpn::RouteInfo<huint32_t>{gateway, ip, netmask_ipv4_bits(32)});
  }

  void
  RoutePoker::DelRoute(huint32_t ip)
  {
    const auto itr = m_PokedRoutes.find(ip);
    if (itr == m_PokedRoutes.end())
      return;
    m_PokedRoutes.erase(itr);

    if (m_Enabled)
      DisableRoute(itr->first, itr->second);
  }

  void
  RoutePoker::Init(AbstractRouter* router, bool enable)
  {
    m_Router = router;
    m_Enabled = enable;
    m_CurrentGateway = {0};
  }

  void
  RoutePoker::DeleteAllRoutes()
  {
    // DelRoute will check enabled, so no need here
    for (const auto& [ip, gateway] : m_PokedRoutes)
      DelRoute(ip);
  }

  void
  RoutePoker::DisableAllRoutes()
  {
    const auto ep = m_Router->hiddenServiceContext().GetDefault();
    m_Router->GetVPNPlatform()->DelDefaultRouteVia(ep->GetNetworkInterface());

    for (const auto& [ip, gateway] : m_PokedRoutes)
      DisableRoute(ip, gateway);
  }

  void
  RoutePoker::EnableAllRoutes()
  {
    for (auto& [ip, gateway] : m_PokedRoutes)
    {
      gateway = m_CurrentGateway;
      EnableRoute(ip, m_CurrentGateway);
    }
  }

  RoutePoker::~RoutePoker()
  {
    const auto vpn = m_Router->GetVPNPlatform();

    for (const auto& [ip, gateway] : m_PokedRoutes)
    {
      if (gateway.h and vpn)
        vpn->DelRoute(vpn::RouteInfo<huint32_t>{gateway, ip, netmask_ipv4_bits(32)});
    }
  }

  std::optional<huint32_t>
  RoutePoker::GetDefaultGateway() const
  {
    if (not m_Router)
      throw std::runtime_error("Attempting to use RoutePoker before calling Init");

    const auto ep = m_Router->hiddenServiceContext().GetDefault();
    const auto gateways =
        m_Router->GetVPNPlatform()->GetDefaultGatewaysNotOn(ep->GetNetworkInterface());
    if (gateways.empty())
    {
      return std::nullopt;
    }
    return gateways[0];
  }

  void
  RoutePoker::Update()
  {
    if (not m_Router)
      throw std::runtime_error("Attempting to use RoutePoker before calling Init");

    const auto maybe = GetDefaultGateway();
    if (not maybe.has_value())
    {
      LogError("Network is down");
      return;
    }
    const huint32_t gateway = *maybe;
    if (m_CurrentGateway != gateway or m_Enabling)
    {
      LogInfo("found default gateway: ", gateway);
      m_CurrentGateway = gateway;

      if (not m_Enabling)  // if route was already set up
        DisableAllRoutes();
      EnableAllRoutes();

      const auto ep = m_Router->hiddenServiceContext().GetDefault();
      m_Router->GetVPNPlatform()->AddDefaultRouteVia(ep->GetNetworkInterface());
    }
  }

  void
  RoutePoker::Enable()
  {
    if (m_Enabled)
      return;

    m_Enabling = true;

    Update();

    m_Enabling = false;
    m_Enabled = true;
  }

  void
  RoutePoker::Disable()
  {
    if (not m_Enabled)
      return;

    DisableAllRoutes();
  }
}  // namespace llarp
