#pragma once

#include <optional>

#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/messaging_base.hpp"

namespace peerpaste::message
{

class GetSuccessorList : public MessagingBase, public Awaitable<std::optional<std::vector<Peer>>>
{
public:
	GetSuccessorList(Peer connection);
	GetSuccessorList(ConcurrentRoutingTable<Peer> *, RequestObject request);
	explicit GetSuccessorList(GetSuccessorList &&other);

	~GetSuccessorList() override;

	void HandleNotification(const RequestObject &request_object) override;

private:
	void create_request() override;
	void handle_request() override;
	void handle_response(RequestObject request_object) override;
	void handle_failed() override;

	std::optional<Peer> connection_;
	std::optional<ConcurrentRoutingTable<Peer> *> routing_table_;
};

} // namespace peerpaste::message
