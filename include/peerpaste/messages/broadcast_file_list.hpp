#pragma once

#include "peerpaste/messaging_base.hpp"
#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/storage.hpp"


namespace peerpaste::message
{

class BroadcastFilelist : public MessagingBase, public Awaitable<MESSAGE_STATE>
{
public:
  BroadcastFilelist(ConcurrentRoutingTable<Peer>* routing_table, StaticStorage* storage);
  BroadcastFilelist(ConcurrentRoutingTable<Peer>* routing_table, StaticStorage* storage, RequestObject request);
  explicit BroadcastFilelist(BroadcastFilelist&& other);

  ~BroadcastFilelist() override;

	void HandleNotification(const RequestObject& request_object) override;
	void HandleNotification(const RequestObject& request_object, HandlerObject<HandlerFunction> handler) override;
	void HandleNotification() override;

private:
  void create_request() override;
  void handle_request() override;
  void handle_response(RequestObject request_object) override;
  void handle_failed() override;

  ConcurrentRoutingTable<Peer>* routing_table_;
  StaticStorage* storage_;
};

} //closing namespace peerpaste::message
