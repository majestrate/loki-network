#pragma once

#include <memory>

#include "privs.hpp"
#include "proxy.hpp"
#include "vpn.hpp"

namespace llarp
{
  /// forward declare
  class CoreDaemon;
}  // namespace llarp

namespace llarp::platform
{
  class Platform
  {
   public:
    virtual ~Platform() = default;

    /// make privs dropper
    virtual std::unique_ptr<PrivDrop>
    MakePrivDrop(uid_t uid, gid_t gid) const = 0;

    /// create vpn platform
    virtual std::shared_ptr<vpn::Platform>
    MakeVPNPlatform() const = 0;

    /// spawn platform process
    /// return proxy to talk with it
    virtual std::unique_ptr<Proxy>
    Spawn() = 0;
  };
}  // namespace llarp::platform
