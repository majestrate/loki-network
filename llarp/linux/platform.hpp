#pragma once
#include "llarp/platform/platform.hpp"

namespace llarp::linux
{
  class Platform : public llarp::platform::Platform
  {
    CoreDaemon* const _parent;

   public:
    explicit Platform(CoreDaemon* parent);

    std::unique_ptr<platform::PrivDrop>
    MakePrivDrop(uid_t uid, gid_t gid) const override;

    std::shared_ptr<vpn::Platform>
    MakeVPNPlatform() const override;

    std::unique_ptr<platform::Proxy>
    Spawn() override;
  };
}  // namespace llarp::linux

namespace llarp::platform
{
  using Impl_t = llarp::linux::Platform;
}
