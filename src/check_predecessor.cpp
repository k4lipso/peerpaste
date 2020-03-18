#include "peerpaste/messages/check_predecessor.hpp"

namespace peerpaste::message
{

CheckPredecessor::CheckPredecessor(ConcurrentRoutingTable<Peer>* routing_table, std::atomic<bool>* checkpre)
        : MessagingBase(MessageType::CHECK_PREDECESSOR)
        ,	routing_table_(routing_table)
        , check_predecessor_flag_(checkpre)
{
}

CheckPredecessor::CheckPredecessor(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request)
        : MessagingBase(MessageType::CHECK_PREDECESSOR, request)
        , routing_table_(routing_table)
        , check_predecessor_flag_(nullptr)
{
}

CheckPredecessor::CheckPredecessor(CheckPredecessor&& other)
        : MessagingBase(std::move(other))
        , routing_table_(other.routing_table_)
        , check_predecessor_flag_(other.check_predecessor_flag_)
{}

CheckPredecessor::~CheckPredecessor()
{
}

void CheckPredecessor::HandleNotification(const RequestObject& request_object)
{
}

void CheckPredecessor::create_request()
{
  Peer target;
  if (not routing_table_->try_get_predecessor(target))
  {
    is_done_ = true;
    Notify();
    return;
  }

  auto notify_message = std::make_shared<Message>(
    Message::create_request("check_predecessor"));
  auto transaction_id = notify_message->get_transaction_id();

  auto handler =
    std::bind(&CheckPredecessor::handle_response, this,
    std::placeholders::_1);

  RequestObject request{ type_ };
  request.set_message(notify_message);
  request.set_connection(std::make_shared<Peer>(target));

  create_handler_object(transaction_id, handler);
  Notify(request, *handler_object_);
  *check_predecessor_flag_ = true;
}

void CheckPredecessor::handle_request()
{
  if(!request_.has_value())
  {
    return;
  }

  auto* transport_object = &request_.value();

  //Generate and push response
  auto message = transport_object->get_message();
  auto response = message->generate_response();
  response->generate_transaction_id();

  RequestObject response_object{ *transport_object };
  response_object.set_message(response);

  Notify(response_object);
  is_done_ = true;
  Notify();
}

void CheckPredecessor::handle_response(RequestObject request_object)
{
  *check_predecessor_flag_ = false;
  if(!request_object.get_message()->is_request())
  {
    is_done_ = true;
    Notify();
    return;
  }

  routing_table_->reset_predecessor();
}


} //closing namespace peerpaste::message
