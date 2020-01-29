#include "peerpaste/messages/notify.hpp"


namespace peerpaste { namespace message
{

Notification::Notification(ConcurrentRoutingTable<Peer>* routing_table)
	: MessagingBase(MessageType::NOTIFICATION)
	,	routing_table_(routing_table)
	, is_request_handler_(false)
{
}

Notification::Notification(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request)
	: MessagingBase(MessageType::NOTIFICATION)
	, routing_table_(routing_table)
	, request_(request)
	, is_request_handler_(true)
{
}

Notification::Notification(Notification&& other)
	: MessagingBase(std::move(other))
	, routing_table_(other.routing_table_)
	, request_(other.request_)
	, is_request_handler_(other.is_request_handler_)
{}

Notification::~Notification()
{
}


void Notification::operator()()
{
  if(is_request_handler_)
  {
    handle_request();
  }
  else
  {
    create_request();
  }
}

void Notification::create_request()
{
  Peer self;

  if(not routing_table_->try_get_self(self)) return;
  if(self.get_id() == "") return;

  Peer target;
  if(not routing_table_->try_get_successor(target)){
      return;
  }

  auto notify_message = std::make_shared<Message>(
                          Message::create_request("notify", { self }));
  auto transaction_id = notify_message->get_transaction_id();

  RequestObject request{ type_ };
  request.set_message(notify_message);
  request.set_connection(std::make_shared<Peer>(target));

  //output_queue_->push(request);
  Notify(request);
  is_done_ = true;
}

void Notification::handle_request()
{
  if(!request_.has_value())
  {
    return;
  }

  auto* transport_object = &(request_.value());

  auto message = transport_object->get_message();
  if(message->get_peers().size() != 1){
      //TODO: handle invalid msg
      util::log(warning, "handle notify invalid message");
      return;
  }

  auto notify_peer = message->get_peers().front();
  if(not routing_table_->has_predecessor()){
      routing_table_->set_predecessor(notify_peer);
      return;
  }

  Peer predecessor;
  Peer self;

  //TODO: if we have a predecessor node we should check if its alive!
  //check the rectify operation in how_to_make_chord_correct.pdf
  if(not routing_table_->try_get_predecessor(predecessor)){
      util::log(warning, "cant handle notify: no predecessor set");
  }
  if(not routing_table_->try_get_self(self)){
      util::log(warning, "cant handle notify: self not set");
  }

  auto predecessor_id = predecessor.get_id();
  auto self_id = self.get_id();
  auto notify_peer_id = notify_peer.get_id();

  if(util::between(predecessor_id, notify_peer_id, self_id)){
      routing_table_->set_predecessor(notify_peer);
  }

  //Generate and push response
  auto response = message->generate_response();
  response->generate_transaction_id();

  RequestObject response_object{*transport_object};
  response_object.set_message(response);
  Notify(response_object);
}


void Notification::HandleNotification(const RequestObject& request_object)
{
}

} //closing namespace message
} //closing namespace peerpaste
