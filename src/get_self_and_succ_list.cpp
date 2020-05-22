#include "peerpaste/messages/get_self_and_succ_list.hpp"


namespace peerpaste::message
{

GetSelfAndSuccList::GetSelfAndSuccList(Peer target)
	: MessagingBase(MessageType::NOTIFICATION)
	,	routing_table_(nullptr)
	, target_(std::move(target))
{
}

GetSelfAndSuccList::GetSelfAndSuccList(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request)
	: MessagingBase(MessageType::NOTIFICATION, request)
	, routing_table_(routing_table)
{
}

GetSelfAndSuccList::GetSelfAndSuccList(GetSelfAndSuccList&& other)
	: MessagingBase(std::move(other))
	, Awaitable(std::move(other))
	, routing_table_(std::move(other.routing_table_))
	, target_(std::move(other.target_))
{}

GetSelfAndSuccList::~GetSelfAndSuccList()
{
}

void GetSelfAndSuccList::HandleNotification(const RequestObject& request_object)
{
}

void GetSelfAndSuccList::create_request()
{
  if(!target_.has_value())
  {
    state_ = MESSAGE_STATE::FAILED;
    set_promise({});
    RequestDestruction();
    return;
  }

  auto get_successor_list_request = std::make_shared<Message>(
                  Message::create_request("get_self_and_successor_list"));

  auto transaction_id = get_successor_list_request->get_transaction_id();

  auto handler = std::bind(&GetSelfAndSuccList::handle_response,
                            this,
                            std::placeholders::_1);

  auto request = RequestObject();
  request.set_message(get_successor_list_request);
  request.set_connection(std::make_shared<Peer>(target_.value()));

  create_handler_object(transaction_id, handler);
  Notify(request, *handler_object_);
}

void GetSelfAndSuccList::handle_request()
{
  if(!request_.has_value())
  {
    state_ = MESSAGE_STATE::FAILED;
    set_promise({});
    RequestDestruction();
    return;
  }

  auto message = request_.value().get_message();

  auto response_message = message->generate_response();
  auto succ_list = routing_table_->get_peers();

  Peer self;
  if(not routing_table_->try_get_self(self))
  {
    util::log(warning, "Could not get self in handle_get_self_and_successor_list_request");
  }

  succ_list.insert(succ_list.begin(), self);
  response_message->set_peers(succ_list);
  response_message->generate_transaction_id();

  auto response = RequestObject{request_.value()};
  response.set_message(response_message);

  Notify(response);
  state_ = MESSAGE_STATE::DONE;
  set_promise({});
  RequestDestruction();
}

void GetSelfAndSuccList::handle_response(RequestObject request_object)
{
  if(request_object.is_request()){
    state_ = MESSAGE_STATE::FAILED;
    set_promise({});
    RequestDestruction();
    return;
  }

  set_promise(request_object.get_message()->get_peers());
  RequestDestruction();
}

void GetSelfAndSuccList::handle_failed()
{
  state_ = MESSAGE_STATE::FAILED;
  set_promise({});
}

} //closing namespace peerpaste::message
