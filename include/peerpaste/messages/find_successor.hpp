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

class FindSuccessor : public MessagingBase, public Awaitable<Peer>
{
public:
  FindSuccessor(ConcurrentRoutingTable<Peer>* routing_table);
  FindSuccessor(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request);
  explicit FindSuccessor(FindSuccessor&& other);

  ~FindSuccessor() override;

  void HandleNotification(const RequestObject& request_object) override;

private:
  void create_request() override;
  void handle_request() override;
  void forward_request();
  void handle_forwarded_response(RequestObject request_object);
  void handle_response(RequestObject request_object) override;
  void handle_failed() override;

	std::unique_ptr<Peer> find_successor(const std::string& id) const;
	std::unique_ptr<Peer> closest_preceding_node(const std::string& id) const;

	ConcurrentRoutingTable<Peer>* routing_table_;
};

} //closing namespace peerpaste::message
