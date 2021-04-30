#include "platform.hpp"
#include "vpn.hpp"

namespace llarp::linux
{
  class PrivDrop : public platform::PrivDrop
  {
   public:
    using platform::PrivDrop::PrivDrop;

    void
    DropPrivs() const override
    {
      if (getuid() or getgid())
        throw std::runtime_error{"cannot drop privs we are not root"};
      if (seteuid(_uid))
        throw std::runtime_error{
            "cannot drop privs, seteuid failed " + std::string{strerror(errno)}};
      if (setegid(_gid))
        throw std::runtime_error{
            "cannot drop privs, setegid failed " + std::string{strerror(errno)}};
    }
  };

  Platform::Platform(CoreDaemon* daemon) : _parent{daemon}
  {}

  std::unique_ptr<platform::PrivDrop>
  Platform::MakePrivDrop(uid_t uid, gid_t gid) const
  {
    return std::make_unique<PrivDrop>(uid, gid);
  }

  std::shared_ptr<vpn::Platform>
  Platform::MakeVPNPlatform() const
  {
    return std::make_shared<vpn::LinuxPlatform>();
  }

  std::unique_ptr<platform::Proxy>
  Platform::Spawn()
  {
    // TODO: implement me
    return nullptr;
  }

}  // namespace llarp::linux
