#include "proxy.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/wait.h>
#include <signal.h>
#include <poll.h>

#include <llarp/util/logging/logger.hpp>

namespace llarp::linux
{
  int
  Proxy::PlatformMainloop(int readfd, int writefd)
  {
    std::array<std::byte, 1024 * 8> buffer{};

    pollfd fds{readfd, POLL_ERR | POLL_IN, 0};

    while (true)
    {
      const auto ret = ::poll(&fds, 1, 1000);
      if (ret == -1)
        return errno;
      if (ret == 0)
      {
        // timeout
        continue;
      }
      else if (fds.revents & POLLERR)
      {
        return 0;
      }
      else if (fds.revents & POLLIN)
      {
        const auto sz = ::read(readfd, buffer.data(), buffer.size());
        if (sz <= 0)
          continue;
      }
    }
  }

  Proxy::Proxy()
  {
    if (pipe2(read_pipe, O_DIRECT | O_CLOEXEC | O_NONBLOCK))
      throw std::runtime_error{"failed to call pipe2: " + std::string{strerror(errno)}};
    if (pipe2(write_pipe, O_DIRECT | O_CLOEXEC | O_NONBLOCK))
      throw std::runtime_error{"failed to call pipe2: " + std::string{strerror(errno)}};
    child_pid = fork();

    if (child_pid == -1)
      throw std::runtime_error{"failed to fork:" + std::string{strerror(errno)}};

    if (child_pid == 0)
    {
      // close read end
      ::close(read_pipe[0]);
      const auto code = PlatformMainloop(write_pipe[0], read_pipe[1]);
      printf("child pid=%d exit code=%d\n", getpid(), code);
      ::close(read_pipe[1]);
      ::exit(code);
    }
    else
    {
      LogInfo("spawned child process pid=", child_pid);
      // close read end
      ::close(write_pipe[0]);
    }
  }

  Proxy::~Proxy()
  {
    if (auto code = kill(child_pid, SIGTERM); code == 0)
    {
      int waitstatus;
      LogInfo("waiting for child process to die pid=", child_pid);
      if (::waitpid(child_pid, &waitstatus, 0) == -1)
        perror("waitpid");
    }
    else
    {
      perror("kill");
    }
  }

  void
  Proxy::Signal(int sig) const
  {
    if (kill(child_pid, sig))
    {
      perror("kill");
    }
    else if (sig == SIGTERM or sig == SIGINT)
    {
      int waitstatus;
      if (waitpid(child_pid, &waitstatus, 0) == -1)
        perror("waitpid");
    }
  }

  int
  Proxy::FD() const
  {
    return read_pipe[0];
  }

  bool
  Proxy::Read()
  {
    // todo: implement me
    return false;
  }

  void
  Proxy::Request(
      platform::Request req, std::function<void(bool, std::vector<std::string_view>)> resultHandler)
  {}

  void
  Proxy::Subscribe(
      platform::Noun noun,
      std::function<void(platform::Notification, std::string_view)> resultHandler)
  {}

}  // namespace llarp::linux
