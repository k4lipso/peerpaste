#include "peerpaste/messages/join.hpp"
#include "peerpaste/messages/query.hpp"
#include "peerpaste/messages/find_successor.hpp"

namespace peerpaste::message
{

Join::Join(ConcurrentRoutingTable<Peer>* routing_table, const std::string& address, const std::string& port)
	: MessagingBase(MessageType::JOIN)
	,	routing_table_(routing_table)
{
	dependencies_.emplace_back(std::make_pair(std::make_shared<Query>(routing_table, address, port), true));
	dependencies_.front().first->Attach(this);
}

Join::Join(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request)
	: MessagingBase(MessageType::JOIN, request)
	, routing_table_(routing_table)
{
}

Join::Join(Join&& other)
	: MessagingBase(std::move(other))
	, Awaitable(std::move(other))
	, routing_table_(std::move(other.routing_table_))
{}

Join::~Join()
{
	set_promise(state_);
}

void Join::HandleNotification(const RequestObject& request_object)
{
	Notify(request_object);
}

void Join::HandleNotification(const RequestObject& request_object, HandlerObject<HandlerFunction> handler)
{
	Notify(request_object, handler);
}
}

void Join::create_request()
{
	(*dependencies_.front().first)();
}

void Join::handle_request()
{
}

void Join::handle_response(RequestObject request_object)
{
}

void Join::handle_failed()
{
}

} //closing namespace peerpaste::message
