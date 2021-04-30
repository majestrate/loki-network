#ifndef LLARP_HPP
#define LLARP_HPP

#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace llarp
{
  namespace vpn
  {
    class Platform;
  }

  class EventLoop;
  struct Config;
  struct RouterContact;
  struct Config;
  struct Crypto;
  struct CryptoManager;
  struct AbstractRouter;
  class NodeDB;

  namespace thread
  {
    class ThreadPool;
  }

  struct RuntimeOptions
  {
    bool background = false;
    bool debug = false;
    bool isSNode = false;
  };

  struct [[deprecated]] Context
  {
    std::shared_ptr<Crypto> crypto = nullptr;
    std::shared_ptr<CryptoManager> cryptoManager = nullptr;
    std::shared_ptr<AbstractRouter> router = nullptr;
    std::shared_ptr<EventLoop> loop = nullptr;
    std::shared_ptr<NodeDB> nodedb = nullptr;
    std::string nodedb_dir;

    virtual ~Context() = default;

    void
    Setup(const RuntimeOptions& opts);

    int
    Run(const RuntimeOptions& opts);

    void
    HandleSignal(int sig);

    /// Configure given the specified config.
    void
    Configure(std::shared_ptr<Config> conf);

    /// handle SIGHUP
    void
    Reload();

    bool
    IsUp() const;

    bool
    LooksAlive() const;

    bool
    IsStopping() const;

    /// close async
    void
    CloseAsync();

    /// wait until closed and done
    void
    Wait();

    /// call a function in logic thread
    /// return true if queued for calling
    /// return false if not queued for calling
    bool
    CallSafe(std::function<void(void)> f);

    /// Creates a router. Can be overridden to allow a different class of router
    /// to be created instead. Defaults to llarp::Router.
    virtual std::shared_ptr<AbstractRouter>
    makeRouter(const std::shared_ptr<EventLoop>& loop);

    /// create the nodedb given our current configs
    virtual std::shared_ptr<NodeDB>
    makeNodeDB();

#ifdef ANDROID

    int androidFD = -1;

    int
    GetUDPSocket();
#endif

   protected:
    std::shared_ptr<Config> config = nullptr;

   private:
    void
    SigINT();

    void
    Close();

    std::unique_ptr<std::promise<void>> closeWaiter;
  };

  namespace platform
  {
    class Platform;
    class Proxy;
  }  // namespace platform

  class PlatformDaemon
  {
    std::shared_ptr<platform::Platform> _platform;

   public:
    /// run priviledged bits
    void
    RunPriviledged();
  };

  class CoreDaemon
  {
    std::shared_ptr<platform::Platform> _platform;
    std::shared_ptr<platform::Proxy> _platformProxy;
    std::shared_ptr<llarp::Config> _config;
    std::shared_ptr<llarp::AbstractRouter> _router;
    std::shared_ptr<llarp::EventLoop> _loop;
    std::shared_ptr<llarp::Crypto> _crypto;
    std::shared_ptr<llarp::CryptoManager> _cryptoManager;

    const llarp::RuntimeOptions _opts;

   public:
    explicit CoreDaemon(std::shared_ptr<llarp::Config> config, const llarp::RuntimeOptions opts);

    virtual ~CoreDaemon() = default;

    virtual std::shared_ptr<AbstractRouter>
    MakeRouter() const;

    virtual std::shared_ptr<EventLoop>
    MakeLoop() const;

    virtual std::shared_ptr<platform::Platform>
    MakePlatform();

    virtual std::shared_ptr<NodeDB>
    MakeNodeDB() const;

    /// drop privileges
    /// throws if it can't
    void
    DropPrivs() const;

    /// configure and setup contexts before priv drop
    void
    Init();

    /// run mainloop
    void
    Run();

    /// handle signal from OS asynchronously
    void
    AsyncHandleSignal(int sig);

    /// router is running
    bool
    Running() const;

    /// router is alive
    bool
    Alive() const;

    /// stop router activity
    void
    Stop();

    /// wait for the router to end at most N ms
    /// return true if we are down
    bool
    WaitForDone(std::chrono::milliseconds N);
  };

}  // namespace llarp

#endif
