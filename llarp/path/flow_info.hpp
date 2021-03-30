#pragma once
#include <llarp/service/convotag.hpp>
#include "path_types.hpp"

namespace llarp::path
{
  struct FlowInfo
  {
    service::ConvoTag tag;
    PathID_t txid;

    bool
    operator==(const FlowInfo& other) const
    {
      return tag == other.tag and txid == other.txid;
    }
  };
}  // namespace llarp::path

namespace std
{
  template <>
  struct hash<llarp::path::FlowInfo>
  {
    size_t
    operator()(const llarp::path::FlowInfo& info) const
    {
      const hash<llarp::service::ConvoTag> h0{};
      const hash<llarp::PathID_t> h1{};
      return h0(info.tag) ^ h1(info.txid);
    }
  };
}  // namespace std
