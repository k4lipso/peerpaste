#include "peerpaste/messages/get_successor_list.hpp"

namespace peerpaste::message
{

GetSuccessorList::GetSuccessorList(Peer connection)
	: MessagingBase(MessageType::GET_SUCCESSOR_LIST)
	, connection_(std::move(connection))
{
}

GetSuccessorList::GetSuccessorList(ConcurrentRoutingTable<Peer> *routing_table, RequestObject request)
	: MessagingBase(MessageType::GET_SUCCESSOR_LIST, request)
	, routing_table_(routing_table)
{
}

GetSuccessorList::GetSuccessorList(GetSuccessorList &&other)
	: MessagingBase(std::move(other))
	, Awaitable(std::move(other))
	, connection_(std::move(other.connection_))
{
}

GetSuccessorList::~GetSuccessorList()
{
}

void GetSuccessorList::HandleNotification(const RequestObject &request_object)
{
}

void GetSuccessorList::create_request()
{
	if(!connection_.has_value())
	{
		state_ = MESSAGE_STATE::FAILED;
		set_promise({});
		RequestDestruction();
		return;
	}

	const auto get_successor_list_request = std::make_shared<Message>(Message::create_request("get_successor_list"));

	const auto transaction_id = get_successor_list_request->get_transaction_id();

	const auto handler = std::bind(&GetSuccessorList::handle_response, this, std::placeholders::_1);

	RequestObject successor_list_request{type_};
	successor_list_request.set_message(get_successor_list_request);
	successor_list_request.set_connection(std::make_shared<Peer>(connection_.value()));

	create_handler_object(transaction_id, handler);
	Notify(successor_list_request, *handler_object_);
}

void GetSuccessorList::handle_request()
{
	if(!routing_table_.has_value())
	{
		state_ = MESSAGE_STATE::FAILED;
		set_promise({});
		RequestDestruction();
		return;
	}

	auto message = request_.value().get_message();

	auto response_message = message->generate_response();
	response_message->set_peers(routing_table_.value()->get_peers());
	response_message->generate_transaction_id();

	RequestObject response{request_.value()};
	response.set_message(response_message);

	Notify(response);
	state_ = MESSAGE_STATE::DONE;
	RequestDestruction();
}

void GetSuccessorList::handle_response(RequestObject request_object)
{
	set_promise(request_object.get_message()->get_peers());
	state_ = MESSAGE_STATE::DONE;
	RequestDestruction();
}

void GetSuccessorList::handle_failed()
{
	set_promise({});
	state_ = MESSAGE_STATE::FAILED;
}

} // namespace peerpaste::message
