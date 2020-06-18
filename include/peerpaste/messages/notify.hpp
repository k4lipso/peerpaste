#pragma once

#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/messaging_base.hpp"

namespace peerpaste::message
{

class Notification : public MessagingBase, public Awaitable<MESSAGE_STATE>
{
public:
	Notification(ConcurrentRoutingTable<Peer> *routing_table);
	Notification(ConcurrentRoutingTable<Peer> *routing_table, RequestObject request);
	explicit Notification(Notification &&other);

	~Notification() override;

	void HandleNotification(const RequestObject &request_object) override;

private:
	void create_request() override;
	void handle_request() override;
	void handle_response(RequestObject request_object) override;
	void handle_failed() override;

	ConcurrentRoutingTable<Peer> *routing_table_;
};

} // namespace peerpaste::message
