#pragma once

#include <sys/types.h>

namespace llarp::platform
{
  class PrivDrop
  {
   protected:
    /// uid to drop to
    const uid_t _uid;
    /// gid to drop to
    const gid_t _gid;

   public:
    /// constructs a priv dropper that will attempt to setuid/setgid to the provided uid/gid
    explicit PrivDrop(uid_t uid, gid_t gid) : _uid{uid}, _gid{gid}
    {}

    /// actually drop the privs
    /// throws if it cannot
    virtual void
    DropPrivs() const = 0;
  };
}  // namespace llarp::platform
