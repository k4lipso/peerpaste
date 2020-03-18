#include "peerpaste/message_factory.hpp"
#include "peerpaste/cryptowrapper.hpp"

#include "peerpaste/messages/notify.hpp"
#include "peerpaste/messages/check_predecessor.hpp"

#include <memory>

namespace peerpaste::message
{

MessageFactory::MessageFactory(ConcurrentRoutingTable<Peer>* routing_table)
	: routing_table_{ routing_table }
{}

std::unique_ptr<MessagingBase> MessageFactory::create_from_request(const RequestObject& request)
{
	if(!request.is_request())
	{
		util::log(debug, "MessageFactory could net create. RequestObject is a Response");
		return nullptr;
	}

	const auto type{ request.get_request_type() };

	if(type == "notify")
	{
		return std::make_unique<Notification>(routing_table_, request);
	}
	if(type == "check_predecessor")
	{
		return std::make_unique<CheckPredecessor>(routing_table_, request);
	}

	if(type == "query")
	{
	}
	if(type == "find_successor")
	{
	}
	if(type == "get_predecessor_and_succ_list")
	{
	}
	if(type == "get_successor_list")
	{
	}
	if(type == "get_self_and_successor_list")
	{
	}
	if(type == "put")
	{
	}
	if(type == "store")
	{
	}
	if(type == "get")
	{
	}
	if(type == "get_internal")
	{
	}
	if(type == "backup")
	{
	}

	return nullptr;
}

} //closing namespace peerpaste::message
