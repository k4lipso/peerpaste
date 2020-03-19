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

class Notification : public MessagingBase
{
public:
  Notification(ConcurrentRoutingTable<Peer>* routing_table);
  Notification(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request);
  explicit Notification(Notification&& other);

  ~Notification() override;

  virtual void HandleNotification(const RequestObject& request_object) override;

private:
  virtual void create_request() override;
  virtual void handle_request() override;
  virtual void handle_response(RequestObject request_object) override;

  ConcurrentRoutingTable<Peer>* routing_table_;
};

} //closing namespace peerpaste::message
