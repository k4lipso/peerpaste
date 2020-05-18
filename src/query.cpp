#include "peerpaste/messages/query.hpp"

namespace peerpaste::message
{

Query::Query(ConcurrentRoutingTable<Peer>* routing_table, const std::string& address, const std::string& port)
	: MessagingBase(MessageType::QUERY)
	,	routing_table_(routing_table)
	, target_("", address, port)
{
}

Query::Query(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request)
	: MessagingBase(MessageType::QUERY, request)
	, routing_table_(routing_table)
{
}

Query::Query(Query&& other)
	: MessagingBase(std::move(other))
	, Awaitable(std::move(other))
	, routing_table_(std::move(other.routing_table_))
	, target_(std::move(other.target_))
{}

Query::~Query()
{
}

void Query::HandleNotification(const RequestObject& request_object)
{
}

void Query::create_request()
{
  Peer self;
  if(not routing_table_->try_get_self(self)){
      util::log(warning, "Cant join, self not set");
  }

  //Generate Query
  const auto query_request = std::make_shared<Message>(
                      Message::create_request("query", { self }));

  const auto transaction_id = query_request->get_transaction_id();

  const auto target = std::make_shared<Peer>(target_);

  const auto handler = std::bind(&Query::handle_response,
                            this,
                            std::placeholders::_1);

  RequestObject request;
  request.set_message(query_request);
  request.set_connection(target);

  create_handler_object(transaction_id, handler);
  Notify(request, *handler_object_);
}

void Query::handle_request()
{
  auto message = request_.value().get_message();
  if(message->get_peers().size() != 1){
    //TODO: handle invalid messages here
  }

  auto peer = message->get_peers().front();
  auto client_ip = request_.value().get_client_ip();
  auto client_port = peer.get_port();
  auto client_id = util::generate_sha256(client_ip, client_port);
  peer.set_ip(client_ip);
  peer.set_id(client_id);

  auto response_message = message->generate_response();
  response_message->add_peer(peer);
  response_message->generate_transaction_id();

  RequestObject response = request_.value();
  response.set_message(response_message);

  Notify(response);
  state_ = MESSAGE_STATE::DONE;
  RequestDestruction();
}

void Query::handle_response(RequestObject request_object)
{
  auto message = request_object.get_message();

  if(message->get_peers().size() == 1)
  {
    auto peer = message->get_peers().front();
    routing_table_->set_self(peer);
    state_ = MESSAGE_STATE::DONE;
    RequestDestruction();
  }
  else
  {
    util::log(warning, "Wrong Message_Size");
    //TODO: handle invalid message
    state_ = MESSAGE_STATE::FAILED;
    RequestDestruction();
  }
}

void Query::handle_failed()
{
  set_promise({});
}


} //closing namespace peerpaste::message
