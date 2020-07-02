#include "peerpaste/messages/check_predecessor.hpp"

namespace peerpaste::message
{

CheckPredecessor::CheckPredecessor(ConcurrentRoutingTable<Peer> *routing_table, std::atomic<bool> *checkpre)
	: MessagingBase(MessageType::CHECK_PREDECESSOR)
	, routing_table_(routing_table)
	, check_predecessor_flag_(checkpre)
{
}

CheckPredecessor::CheckPredecessor(ConcurrentRoutingTable<Peer> *routing_table, RequestObject request)
	: MessagingBase(MessageType::CHECK_PREDECESSOR, request)
	, routing_table_(routing_table)
	, check_predecessor_flag_(nullptr)
{
}

CheckPredecessor::CheckPredecessor(CheckPredecessor &&other)
	: MessagingBase(std::move(other))
	, routing_table_(other.routing_table_)
	, check_predecessor_flag_(other.check_predecessor_flag_)
{
}

CheckPredecessor::~CheckPredecessor()
{
	set_promise(state_);
}

void CheckPredecessor::HandleNotification(const RequestObject &request_object)
{
}

void CheckPredecessor::create_request()
{
	std::scoped_lock lk{mutex_};
	Peer target;
	if(not routing_table_->try_get_predecessor(target))
	{
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	auto notify_message = std::make_shared<Message>(Message::create_request("check_predecessor"));
	auto transaction_id = notify_message->get_transaction_id();

	auto handler = std::bind(&CheckPredecessor::handle_response, this, std::placeholders::_1);

	RequestObject request{type_};
	request.set_message(notify_message);
	request.set_connection(std::make_shared<Peer>(target));

	create_handler_object(transaction_id, handler);
	Notify(request, *handler_object_);
	*check_predecessor_flag_ = true;
}

void CheckPredecessor::handle_request()
{
	std::scoped_lock lk{mutex_};
	if(!request_.has_value())
	{
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	auto *transport_object = &request_.value();

	// Generate and push response
	auto message = transport_object->get_message();
	auto response = message->generate_response();
	response->generate_transaction_id();

	RequestObject response_object{*transport_object};
	response_object.set_message(response);

	Notify(response_object);
	state_ = MESSAGE_STATE::DONE;
	RequestDestruction();
}

void CheckPredecessor::handle_response(RequestObject request_object)
{
	std::scoped_lock lk{mutex_};
	*check_predecessor_flag_ = false;
	if(!request_object.get_message()->is_request())
	{
		state_ = MESSAGE_STATE::DONE;
		RequestDestruction();
		return;
	}

	state_ = MESSAGE_STATE::FAILED;
	routing_table_->reset_predecessor();
}

void CheckPredecessor::handle_failed()
{
	state_ = MESSAGE_STATE::FAILED;
	routing_table_->reset_predecessor();
	*check_predecessor_flag_ = false;
}

} // namespace peerpaste::message
