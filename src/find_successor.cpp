#include "peerpaste/messages/find_successor.hpp"

namespace peerpaste::message
{

FindSuccessor::FindSuccessor(ConcurrentRoutingTable<Peer>* routing_table, const std::string& id)
	: MessagingBase(MessageType::FIND_SUCCESSOR)
	, id_(id)
	,	routing_table_(routing_table)
{
}

FindSuccessor::FindSuccessor(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request)
	: MessagingBase(MessageType::FIND_SUCCESSOR, request)
	, routing_table_(routing_table)
{
}

FindSuccessor::FindSuccessor(FindSuccessor&& other)
	: MessagingBase(std::move(other))
	, Awaitable(std::move(other))
	, id_(std::move(other.id_))
	, routing_table_(std::move(other.routing_table_))
{}

FindSuccessor::~FindSuccessor()
{
}

void FindSuccessor::HandleNotification(const RequestObject& request_object)
{
}

void FindSuccessor::create_request()
{
	const auto successor_ptr = find_successor(id_);

	if(successor_ptr)
	{
		set_promise(*successor_ptr.get());
		state_ = MESSAGE_STATE::DONE;
		RequestDestruction();
		return;
	}

  auto find_succ_request =
      std::make_shared<Message>(Message::create_request(
                                    "find_successor", { Peer(id_, "", "")}));
  auto transaction_id = find_succ_request->generate_transaction_id();

	auto handler = std::bind(&FindSuccessor::handle_response,
																			this,
																			std::placeholders::_1);

	RequestObject successor_request{type_};
	successor_request.set_message(find_succ_request);
	successor_request.set_connection(closest_preceding_node(id_));

  create_handler_object(transaction_id, handler);
  Notify(successor_request, *handler_object_);
}

void FindSuccessor::handle_request()
{
  if(!request_.has_value())
  {
    state_ = MESSAGE_STATE::FAILED;
    RequestDestruction();
    return;
  }

  auto message = request_->get_message();
  if(message->get_peers().size() != 1){
    //TODO: handle invalid message here
  }

  auto id = message->get_peers().front().get_id();
  auto successor = find_successor(id);

  if(successor == nullptr)
  {
    forward_request();
    return;
  }

  auto response_message = message->generate_response();

  response_message->set_peers( { *successor.get() } );
  response_message->generate_transaction_id();

  //auto response = std::make_shared<RequestObject>(*transport_object);
  //response->set_message(response_message);
  RequestObject response(request_.value());
  response.set_message(response_message);

  //push_to_write_queue(response);
  Notify(response);
  state_ = MESSAGE_STATE::DONE;
  RequestDestruction();
}

void FindSuccessor::forward_request()
{
  if(!request_.has_value())
  {
    state_ = MESSAGE_STATE::FAILED;
    RequestDestruction();
    return;
  }

  auto message = request_->get_message();
  auto id = message->get_peers().front().get_id();

  //We have to use aggregator
  //to request successor remotely befor responding
  std::shared_ptr<Peer> successor_predecessor = closest_preceding_node(id);
  auto new_request = std::make_shared<Message>(
    Message::create_request("find_successor",
                            { message->get_peers().front() }));
  auto transaction_id = new_request->generate_transaction_id();

  //Request that requests successor from another peer
  RequestObject request{type_};
  request.set_message(new_request);
  request.set_connection(successor_predecessor);

  //Response that will be sent back when request above
  //got answered
  RequestObject response(request_.value());
  response.set_message(message->generate_response());

  //aggregator_.add_aggregat(response, { transaction_id });
  auto handler = std::bind(&FindSuccessor::handle_forwarded_response, this, std::placeholders::_1);

  create_handler_object(transaction_id, handler);
  Notify(request, *handler_object_);
  return;
}

void FindSuccessor::handle_forwarded_response(RequestObject request_object)
{
  if(!request_.has_value())
  {
    state_ = MESSAGE_STATE::FAILED;
    RequestDestruction();
    return;
  }

  const auto message = request_object.get_message();

  auto successor = message->get_peers().front();

  auto response_message = message->generate_response();

  response_message->set_peers( { successor } );
  response_message->generate_transaction_id();

  RequestObject response(request_.value());
  response.set_message(response_message);

  Notify(response);
  state_ = MESSAGE_STATE::DONE;
  RequestDestruction();
}

void FindSuccessor::handle_response(RequestObject request_object)
{
  const auto message = request_object.get_message();
  if(message->get_peers().size() != 1)
  {
      //TODO: handle invalid message
    state_ = MESSAGE_STATE::FAILED;
    util::log(debug, "invalid find_successor response");
  }

	state_ = MESSAGE_STATE::DONE;
	set_promise(message->get_peers().front());
	RequestDestruction();
}

void FindSuccessor::handle_failed()
{
	set_promise({});
}

std::unique_ptr<Peer> FindSuccessor::find_successor(const std::string& id) const
{
  Peer self;
  if(not routing_table_->try_get_self(self)){
    util::log(warning, "Cant find successor, self was not set");
    return nullptr;
  }

  //if peers are empty there is no successor
  if(routing_table_->size() == 0){
    auto predecessor = closest_preceding_node(id);
    if(predecessor->get_id() == self.get_id()){
      return std::make_unique<Peer>(self);
    }
    return nullptr;
  }

  auto self_id = self.get_id();
  Peer successor;
  if(not routing_table_->try_get_successor(successor)){
    util::log(warning, "Cant find successor, successor was not set");
    return nullptr;
  }
  auto succ_id = successor.get_id();
  if(util::between(self_id, id, succ_id) || id == succ_id)
  {
    return std::make_unique<Peer>(successor);
  } else {
    auto predecessor = closest_preceding_node(id);
    if(predecessor->get_id() == self.get_id()){
      return std::make_unique<Peer>(self);
    }
    return nullptr;
  }
}

std::unique_ptr<Peer> FindSuccessor::closest_preceding_node(const std::string& id) const
{
    Peer self;
    if(not routing_table_->try_get_self(self)){
        util::log(warning, "Cant find closest_preceding_node, self was not set");
        return nullptr;
    }
    auto peers = routing_table_->get_peers();
    for(int i = peers.size() - 1; i >= 0; i--)
    {
        std::string peer_id = peers.at(i).get_id();
        if(util::between(self.get_id(), peer_id, id)){
            return std::make_unique<Peer>(peers.at(i));
        }
    }
    return std::make_unique<Peer>(self);
}

} //closing namespace peerpaste::message
