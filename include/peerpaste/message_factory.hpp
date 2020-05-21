#pragma once

#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/messaging_base.hpp"

namespace peerpaste::message
{

class MessageFactory
{
public:
  MessageFactory() = delete;
  MessageFactory(ConcurrentRoutingTable<Peer>* routing_table);

  std::unique_ptr<MessagingBase> create_from_request(const RequestObject& request);

private:
  ConcurrentRoutingTable<Peer>* routing_table_;
};

} // closing namespace peerpaste::message