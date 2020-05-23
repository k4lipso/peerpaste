#pragma once

#include <optional>

#include "peerpaste/messaging_base.hpp"
#include "peerpaste/concurrent_routing_table.hpp"


namespace peerpaste::message
{

class GetSelfAndSuccList : public MessagingBase, public Awaitable<std::optional<std::vector<Peer>>>
{
public:
  GetSelfAndSuccList(Peer target);
  GetSelfAndSuccList(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request);
  explicit GetSelfAndSuccList(GetSelfAndSuccList&& other);

  ~GetSelfAndSuccList() override;

  void HandleNotification(const RequestObject& request_object) override;

private:
  void create_request() override;
  void handle_request() override;
  void handle_response(RequestObject request_object) override;
  void handle_failed() override;

  ConcurrentRoutingTable<Peer>* routing_table_;
  std::optional<Peer> target_;
};

} //closing namespace peerpaste::message
