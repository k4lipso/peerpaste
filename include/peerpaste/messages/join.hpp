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

class Join : public MessagingBase, public Awaitable<MESSAGE_STATE>
{
public:
  Join(ConcurrentRoutingTable<Peer>* routing_table, const std::string& address, const std::string& port);
  Join(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request);
  explicit Join(Join&& other);

  ~Join() override;

  void HandleNotification(const RequestObject& request_object) override;
  void HandleNotification(const RequestObject& request_object, HandlerObject<HandlerFunction> handler) override;
  void HandleNotification() override;

private:
  void handle_query_notify(MessagingBase* MessagePtr);
  void handle_find_successor_notify(MessagingBase* MessagePtr);
  void handle_get_successor_list_notify(MessagingBase* MessagePtr);

  void create_request() override;
  void handle_request() override;
  void handle_response(RequestObject request_object) override;
  void handle_failed() override;

  ConcurrentRoutingTable<Peer>* routing_table_;
  Peer target_;
};

} //closing namespace peerpaste::message
