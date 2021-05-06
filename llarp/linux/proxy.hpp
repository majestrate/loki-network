#pragma once

#include <llarp/platform/proxy.hpp>

namespace llarp::linux
{
  class Proxy final : public platform::Proxy
  {
    static int
    PlatformMainloop(int readfd, int writefd);

    int read_pipe[2];
    int write_pipe[2];
    pid_t child_pid;

   public:
    Proxy();
    ~Proxy();

    void
    Request(
        platform::Request req,
        std::function<void(bool, std::vector<std::string_view>)> resultHandler) override;

    void
    Subscribe(
        platform::Noun noun,
        std::function<void(platform::Notification, std::string_view)> resultHandler) override;

    int
    FD() const override;

    bool
    Read() override;

    void
    Signal(int sig) const override;
  };
}  // namespace llarp::linux
