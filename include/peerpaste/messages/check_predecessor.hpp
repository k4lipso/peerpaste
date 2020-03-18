#pragma once

#include "peerpaste/messaging_base.hpp"
#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/peer.hpp"

namespace peerpaste::message
{

class CheckPredecessor : public MessagingBase
{
public:
  CheckPredecessor(ConcurrentRoutingTable<Peer>* routing_table, std::atomic<bool>* checkpre);
  CheckPredecessor(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request);
  explicit CheckPredecessor(CheckPredecessor&& other);

  ~CheckPredecessor() override;

  virtual void HandleNotification(const RequestObject& request_object) override;

private:
  virtual void create_request() override;
  virtual void handle_request() override;
  virtual void handle_response(RequestObject request_object) override;

  ConcurrentRoutingTable<Peer>* routing_table_;
  std::atomic<bool>* check_predecessor_flag_;
};

} //closing namespace peerpaste::message
