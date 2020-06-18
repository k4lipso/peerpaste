#pragma once

#include <optional>

#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/messaging_base.hpp"

namespace peerpaste::message
{

class Query : public MessagingBase, public Awaitable<std::optional<Peer>>
{
public:
	Query(ConcurrentRoutingTable<Peer> *routing_table, const std::string &address, const std::string &port);
	Query(ConcurrentRoutingTable<Peer> *routing_table, RequestObject request);
	explicit Query(Query &&other);

	~Query() override;

	void HandleNotification(const RequestObject &request_object) override;

private:
	void create_request() override;
	void handle_request() override;
	void handle_response(RequestObject request_object) override;
	void handle_failed() override;

	ConcurrentRoutingTable<Peer> *routing_table_;

	Peer target_;
};

} // namespace peerpaste::message
