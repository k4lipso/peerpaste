#pragma once

#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/messaging_base.hpp"
#include "peerpaste/peer.hpp"

namespace peerpaste::message
{

class CheckPredecessor : public MessagingBase, public Awaitable<MESSAGE_STATE>
{
public:
	CheckPredecessor(ConcurrentRoutingTable<Peer> *routing_table, std::atomic<bool> *checkpre);
	CheckPredecessor(ConcurrentRoutingTable<Peer> *routing_table, RequestObject request);
	explicit CheckPredecessor(CheckPredecessor &&other);

	~CheckPredecessor() override;

	void HandleNotification(const RequestObject &request_object) override;

private:
	void create_request() override;
	void handle_request() override;
	void handle_response(RequestObject request_object) override;

	void handle_failed() override;

	ConcurrentRoutingTable<Peer> *routing_table_;
	std::atomic<bool> *check_predecessor_flag_;
};

} // namespace peerpaste::message
