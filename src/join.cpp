#include "peerpaste/messages/join.hpp"
#include "peerpaste/messages/query.hpp"
#include "peerpaste/messages/find_successor.hpp"
#include "peerpaste/messages/get_successor_list.hpp"

namespace peerpaste::message
{

Join::Join(ConcurrentRoutingTable<Peer>* routing_table, const std::string& address, const std::string& port)
	: MessagingBase(MessageType::JOIN)
	,	routing_table_(routing_table)
	, target_(Peer("", address, port))
{
	dependencies_.emplace_back(std::make_pair(std::make_shared<Query>(routing_table, address, port), true));
	dependencies_.front().first->Attach(this);
}

Join::Join(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request)
	: MessagingBase(MessageType::JOIN, request)
	, routing_table_(routing_table)
{
}

Join::Join(Join&& other)
	: MessagingBase(std::move(other))
	, Awaitable(std::move(other))
	, routing_table_(std::move(other.routing_table_))
	, target_(std::move(other.target_))
{}

Join::~Join()
{
	set_promise(state_);
}

void Join::HandleNotification(const RequestObject& request_object)
{
	Notify(request_object);
}

void Join::HandleNotification(const RequestObject& request_object, HandlerObject<HandlerFunction> handler)
{
	Notify(request_object, handler);
}

void Join::HandleNotification()
{
  const auto MessagePtr =  dependencies_.front().first;

  if(MessagePtr == nullptr)
  {
    state_ = MESSAGE_STATE::FAILED;
    RequestDestruction();
    return;
  }

  if(!MessagePtr->is_done() || MessagePtr->get_state() != MESSAGE_STATE::DONE)
  {
    state_ = MESSAGE_STATE::FAILED;
    RequestDestruction();
    return;
  }

  if(MessagePtr->GetType() == MessageType::QUERY)
  {
    handle_query_notify(MessagePtr.get());
  }
  else if(MessagePtr->GetType() == MessageType::FIND_SUCCESSOR)
  {
    handle_find_successor_notify(MessagePtr.get());
  }
  else if(MessagePtr->GetType() == MessageType::GET_SUCCESSOR_LIST)
  {
    handle_get_successor_list_notify(MessagePtr.get());
  }
}

void Join::handle_query_notify(MessagingBase* MessagePtr)
{
  const auto self = dynamic_cast<Query*>(MessagePtr)->get_future().get().value();

  dependencies_.clear();
  time_point_ = std::chrono::system_clock::now() + DURATION;
  dependencies_.emplace_back(std::make_pair(
                               std::make_shared<FindSuccessor>(routing_table_,
                                                               target_,
                                                               self.get_id()),
                               true));

  dependencies_.front().first->Attach(this);
  (*dependencies_.front().first)();
}

void Join::handle_find_successor_notify(MessagingBase* MessagePtr)
{
  const auto successor = dynamic_cast<FindSuccessor*>(MessagePtr)->get_future().get().value();
  routing_table_->set_successor(successor);

  dependencies_.clear();
  time_point_ = std::chrono::system_clock::now() + DURATION;
  dependencies_.emplace_back(std::make_pair(
                               std::make_shared<GetSuccessorList>(successor),
                               true));

  dependencies_.front().first->Attach(this);
  //dependencies_.front().first->create_request();
  (*dependencies_.front().first)();
}

void Join::handle_get_successor_list_notify(MessagingBase* MessagePtr)
{
  const auto successor_list = dynamic_cast<GetSuccessorList*>(MessagePtr)->get_future().get().value();

  routing_table_->replace_after_successor(successor_list);

  state_ = MESSAGE_STATE::DONE;
  RequestDestruction();
}

void Join::create_request()
{
  (*dependencies_.front().first)();
}

void Join::handle_request()
{
}

void Join::handle_response(RequestObject request_object)
{
}

void Join::handle_failed()
{
  state_ = MESSAGE_STATE::FAILED;
}

} //closing namespace peerpaste::message
