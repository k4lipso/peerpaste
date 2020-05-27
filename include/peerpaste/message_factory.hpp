#pragma once

#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/storage.hpp"
#include "peerpaste/messaging_base.hpp"

namespace peerpaste::message
{

class MessageFactory
{
public:
  MessageFactory() = delete;
  MessageFactory(ConcurrentRoutingTable<Peer>* routing_table, StaticStorage* storage);

  std::unique_ptr<MessagingBase> create_from_request(const RequestObject& request);

  template<typename MessageType, typename... ArgsT>
  std::unique_ptr<MessageType> create_request(ArgsT&&... Args)
  {
    return std::make_unique<MessageType>(std::forward<ArgsT>(Args)...);
  }

private:
  ConcurrentRoutingTable<Peer>* routing_table_;
  StaticStorage* storage_;
};

} // closing namespace peerpaste::message
