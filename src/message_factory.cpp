#include "peerpaste/message_factory.hpp"
#include "peerpaste/cryptowrapper.hpp"

#include "peerpaste/messages/broadcast_file_list.hpp"
#include "peerpaste/messages/check_predecessor.hpp"
#include "peerpaste/messages/find_successor.hpp"
#include "peerpaste/messages/get_file.hpp"
#include "peerpaste/messages/get_pred_and_succ_list.hpp"
#include "peerpaste/messages/get_self_and_succ_list.hpp"
#include "peerpaste/messages/get_successor_list.hpp"
#include "peerpaste/messages/notify.hpp"
#include "peerpaste/messages/query.hpp"

#include <memory>

namespace peerpaste::message
{

MessageFactory::MessageFactory(ConcurrentRoutingTable<Peer> *routing_table, StaticStorage *storage)
	: routing_table_{routing_table}
	, storage_{storage}
{
}

std::unique_ptr<MessagingBase> MessageFactory::create_from_request(const RequestObject &request)
{
	if(!request.is_request())
	{
		util::log(debug, "MessageFactory could net create. RequestObject is a Response");
		return nullptr;
	}

	const auto type{request.get_request_type()};

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
		return std::make_unique<Query>(routing_table_, request);
	}
	if(type == "find_successor")
	{
		return std::make_unique<FindSuccessor>(routing_table_, request);
	}
	if(type == "get_successor_list")
	{
		return std::make_unique<GetSuccessorList>(routing_table_, request);
	}
	if(type == "get_predecessor_and_succ_list")
	{
		return std::make_unique<GetPredAndSuccList>(routing_table_, request);
	}
	if(type == "get_self_and_successor_list")
	{
		return std::make_unique<GetSelfAndSuccList>(routing_table_, request);
	}
	if(type == "broadcast_filelist")
	{
		return std::make_unique<BroadcastFilelist>(routing_table_, storage_, request);
	}
	if(type == "get_file")
	{
		return std::make_unique<GetFile>(storage_, request);
	}

	return nullptr;
}

} // namespace peerpaste::message
