#pragma once

#include <functional>
#include <vector>

namespace llarp::platform
{
  /// what platform entities we are working with
  enum class Noun
  {
    /// a network interfaces
    eNetworkInterface,
    /// addresses on network interfaces
    eInterfaceAddress,
    /// routing table entries
    eRouteSpec
  };

  /// what things we can do with platform entities
  enum class Verb
  {
    /// add a new entity
    eAdd,
    /// delete an existing entity if we can
    eDelete,
    /// list existing entities at the current moment
    eList
  };

  /// a command we send to the platform specific bits
  struct Request
  {
    /// what type of entity we are acting on
    Noun entity;
    /// what are we doing with the entity we are acting on
    Verb action;
    /// the parameters if they exist
    std::vector<std::vector<std::byte>> params;
  };

  /// a notification event type
  /// tells what happened to an entity
  enum class Event
  {
    /// entity was added
    eAdded,
    /// entity was removed
    eRemoved,
    /// entity had a property changed from old value to a new value
    eUpdated
  };

  /// a notification about changes to an entity
  struct Notification
  {
    /// what entity we are talking about
    Noun entity;
    /// what happened to the entity
    Event what;
    /// the parameters for this event if they exist
    std::vector<std::vector<std::byte>> params;
  };

  /// proxy for talking with the platform process
  class Proxy
  {
   public:
    virtual ~Proxy() = default;

    /// send a request
    /// handle response with a handler called with (success, return_values)
    virtual void
    Request(
        Request req, std::function<void(bool, std::vector<std::vector<std::byte>>)> handler) = 0;

    /// subscribe to notifications for a noun
    virtual void
    Subscribe(Noun noun, std::function<void(Notification)> handler) = 0;

    /// subscribe to all notifiactions for all nouns
    virtual void
    SubscribeAll(std::function<void(Notification)> handler) = 0;
  };

}  // namespace llarp::platform
