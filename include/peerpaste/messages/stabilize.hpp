#pragma once

#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/messaging_base.hpp"

namespace peerpaste::message
{

class Stabilize : public MessagingBase, public Awaitable<MESSAGE_STATE>
{
public:
	Stabilize(ConcurrentRoutingTable<Peer> *routing_table, std::atomic<bool> *stabilize_flag);
	Stabilize(ConcurrentRoutingTable<Peer> *routing_table, RequestObject request);
	explicit Stabilize(Stabilize &&other);

	~Stabilize() override;

	void HandleNotification(const RequestObject &request_object) override;
	void HandleNotification(const RequestObject &request_object, HandlerObject<HandlerFunction> handler) override;
	void HandleNotification() override;

private:
	void handle_get_pred_and_succ_list_notify(MessagingBase *MessagePtr);
	void handle_get_self_and_succ_list_notify(MessagingBase *MessagePtr);
	void handle_notification_notify(MessagingBase *MessagePtr);

	void create_notification();

	void create_request() override;
	void handle_request() override;
	void handle_response(RequestObject request_object) override;
	void handle_failed() override;

	ConcurrentRoutingTable<Peer> *routing_table_;
	std::atomic<bool> *stabilize_flag_;
};

} // namespace peerpaste::message
