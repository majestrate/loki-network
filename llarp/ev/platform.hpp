#pragma once

#include <unistd.h>

namespace llarp::platform
{
  /// the platform specific bits themself
  class PrivilegedProcess
  {
   public:
  };

  /// a proxy class that talks to the platform specific process
  class UnprivilegedClient
  {};

}  // namespace llarp::platform
