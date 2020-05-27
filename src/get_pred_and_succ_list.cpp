#include "peerpaste/messages/get_pred_and_succ_list.hpp"


namespace peerpaste::message
{

GetPredAndSuccList::GetPredAndSuccList(Peer target)
	: MessagingBase(MessageType::GET_PRED_AND_SUCC_LIST)
	,	routing_table_(nullptr)
	, target_(std::move(target))
{
}

GetPredAndSuccList::GetPredAndSuccList(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request)
	: MessagingBase(MessageType::GET_PRED_AND_SUCC_LIST, request)
	, routing_table_(routing_table)
{
}

GetPredAndSuccList::GetPredAndSuccList(GetPredAndSuccList&& other)
	: MessagingBase(std::move(other))
	, Awaitable(std::move(other))
	, routing_table_(std::move(other.routing_table_))
	, target_(std::move(other.target_))
{}

GetPredAndSuccList::~GetPredAndSuccList()
{
}

void GetPredAndSuccList::HandleNotification(const RequestObject& request_object)
{
}

void GetPredAndSuccList::create_request()
{
  if(!target_.has_value())
  {
    state_ = MESSAGE_STATE::FAILED;
    set_promise({});
    RequestDestruction();
    return;
  }

  auto get_predecessor_msg = std::make_shared<Message>(
                      Message::create_request("get_predecessor_and_succ_list"));

  auto transaction_id = get_predecessor_msg->get_transaction_id();

  auto handler = std::bind(&GetPredAndSuccList::handle_response,
                           this,
                           std::placeholders::_1);

  auto request = RequestObject();
  request.set_message(get_predecessor_msg);
  request.set_connection(std::make_shared<Peer>(target_.value()));

  create_handler_object(transaction_id, handler);
  Notify(request, *handler_object_);
}

void GetPredAndSuccList::handle_request()
{
  if(!request_.has_value())
  {
    state_ = MESSAGE_STATE::FAILED;
    set_promise({});
    RequestDestruction();
    return;
  }

  auto message = request_.value().get_message();
  if(message->get_peers().size() != 0){
      //TODO: handle_invalid_message!
      util::log(warning, "Invalid message handle_get_predecessor_req");
  }

  auto response = message->generate_response();

  Peer predecessor;
  if(routing_table_->try_get_predecessor(predecessor))
  {
    auto peers = routing_table_->get_peers();
    peers.insert(peers.begin(), predecessor);
    response->set_peers(peers);
  }

  response->generate_transaction_id();

  auto response_object = RequestObject(request_.value());
  response_object.set_message(response);

  Notify(response_object);
  state_ = MESSAGE_STATE::DONE;
  set_promise({});
  RequestDestruction();
}

void GetPredAndSuccList::handle_response(RequestObject request_object)
{
  if(request_object.is_request())
  {
    state_ = MESSAGE_STATE::FAILED;
    set_promise({});
    RequestDestruction();
    return;
  }

  state_ = MESSAGE_STATE::DONE;
  set_promise(request_object.get_message()->get_peers());
  RequestDestruction();
}

void GetPredAndSuccList::handle_failed()
{
  state_ = MESSAGE_STATE::FAILED;
  set_promise({});
}

} //closing namespace peerpaste::message
