#pragma once

#include <optional>

#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/messaging_base.hpp"

namespace peerpaste::message
{

class GetPredAndSuccList : public MessagingBase, public Awaitable<std::optional<std::vector<Peer>>>
{
public:
	GetPredAndSuccList(Peer target);
	GetPredAndSuccList(ConcurrentRoutingTable<Peer> *routing_table, RequestObject request);
	explicit GetPredAndSuccList(GetPredAndSuccList &&other);

	~GetPredAndSuccList() override;

	void HandleNotification(const RequestObject &request_object) override;

private:
	void create_request() override;
	void handle_request() override;
	void handle_response(RequestObject request_object) override;
	void handle_failed() override;

	ConcurrentRoutingTable<Peer> *routing_table_;
	std::optional<Peer> target_;
};

} // namespace peerpaste::message
