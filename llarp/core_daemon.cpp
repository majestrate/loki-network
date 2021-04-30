#include <llarp.hpp>
#include "llarp/crypto/crypto_libsodium.hpp"
#include "llarp/config/config.hpp"
#include "llarp/platform/platform.hpp"
#include "llarp/router/router.hpp"

#ifdef _WIN32
#include "llarp/win32/platform.hpp"
#endif

#ifdef __linux__
#ifdef ANDROID
#include "llarp/android/platform.hpp"
#else
#include "llarp/linux/platform.hpp"
#endif
#endif

#ifdef __APPLE__
#include "llarp/apple/platform.hpp"
#endif

namespace llarp
{
  CoreDaemon::CoreDaemon(std::shared_ptr<Config> config, const RuntimeOptions opts)
      : _config{std::move(config)}
      , _crypto{std::make_shared<sodium::CryptoLibSodium>()}
      , _opts{opts}
  {
    _cryptoManager = std::make_shared<CryptoManager>(_crypto.get());
  }

  void
  CoreDaemon::Init()
  {
    llarp::LogInfo(llarp::VERSION_FULL, " ", llarp::RELEASE_MOTTO);
    _platform = MakePlatform();
    _loop = MakeLoop();
    _router = MakeRouter();
  }

  void
  CoreDaemon::DropPrivs() const
  {
    std::unique_ptr<platform::PrivDrop> drop;
    if (auto maybe_gid = _config->router.m_gid)
    {
      if (auto maybe_uid = _config->router.m_uid)
      {
        LogInfo("drop privs to uid=", *maybe_uid, " gid=", *maybe_gid);
        drop = _platform->MakePrivDrop(*maybe_uid, *maybe_gid);
      }
    }
    if (drop)
    {
      drop->DropPrivs();
    }
  }

  bool
  CoreDaemon::Alive() const
  {
    return _router and _router->LooksAlive();
  }

  bool
  CoreDaemon::Running() const
  {
    return _router and _router->IsRunning();
  }

  void
  CoreDaemon::AsyncHandleSignal(int sig)
  {
    if (_loop and _router and (sig == SIGINT or sig == SIGTERM))
      _loop->call_soon([r = _router]() { r->Stop(); });
  }

  void
  CoreDaemon::Run()
  {
    if (not _router->Configure(_config, _opts.isSNode, MakeNodeDB()))
      throw std::runtime_error{"failed to configure router"};
    if (not _router->Run())
      throw std::runtime_error{"failed to run router"};
    _loop->run();
  }

  std::shared_ptr<NodeDB>
  CoreDaemon::MakeNodeDB() const
  {
    fs::path nodedb_dir{_config->router.m_dataDir / "nodedb"};
    return std::make_shared<NodeDB>(
        nodedb_dir, [r = _router](auto call) { r->QueueDiskIO(std::move(call)); });
  }

  std::shared_ptr<AbstractRouter>
  CoreDaemon::MakeRouter() const
  {
    return std::make_shared<Router>(_loop, _platform->MakeVPNPlatform());
  }

  std::shared_ptr<EventLoop>
  CoreDaemon::MakeLoop() const
  {
    auto jobQueueSize = std::max(event_loop_queue_size, _config->router.m_JobQueueSize);
    return EventLoop::create(jobQueueSize);
  }

  std::shared_ptr<platform::Platform>
  CoreDaemon::MakePlatform()
  {
    return std::make_shared<platform::Impl_t>(this);
  }

}  // namespace llarp
