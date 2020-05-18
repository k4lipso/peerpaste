#pragma once

#include <optional>

#include "peerpaste/message.hpp"
#include "peerpaste/peer.hpp"
#include "peerpaste/boost_session.hpp"
#include "peerpaste/request_object.hpp"

#include "peerpaste/concurrent_queue.hpp"
#include "peerpaste/messaging_base.hpp"
#include "peerpaste/thread_pool.hpp"
#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/peer.hpp"


namespace peerpaste::message
{

class Query : public MessagingBase, public Awaitable<std::optional<Peer>>
{
public:
  Query(ConcurrentRoutingTable<Peer>* routing_table, const std::string& address, const std::string& port);
  Query(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request);
  explicit Query(Query&& other);

  ~Query() override;

  void HandleNotification(const RequestObject& request_object) override;

private:
  void create_request() override;
  void handle_request() override;
  void handle_response(RequestObject request_object) override;
  void handle_failed() override;

  ConcurrentRoutingTable<Peer>* routing_table_;

  Peer target_;
};

} //closing namespace peerpaste::message
