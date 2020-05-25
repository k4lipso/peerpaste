#include "peerpaste/messages/stabilize.hpp"
#include "peerpaste/messages/get_pred_and_succ_list.hpp"
#include "peerpaste/messages/get_self_and_succ_list.hpp"
#include "peerpaste/messages/notify.hpp"


namespace peerpaste::message
{

Stabilize::Stabilize(ConcurrentRoutingTable<Peer>* routing_table, std::atomic<bool>* stabilize_flag)
	: MessagingBase(MessageType::STABILIZE)
	,	routing_table_(routing_table)
	, stabilize_flag_(stabilize_flag)
{
}

Stabilize::Stabilize(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request)
	: MessagingBase(MessageType::STABILIZE, request)
	, routing_table_(routing_table)
	, stabilize_flag_(nullptr)
{
}

Stabilize::Stabilize(Stabilize&& other)
	: MessagingBase(std::move(other))
	, Awaitable(std::move(other))
	, routing_table_(std::move(other.routing_table_))
	, stabilize_flag_(other.stabilize_flag_)
{}

Stabilize::~Stabilize()
{
	set_promise(state_);
}

void Stabilize::HandleNotification(const RequestObject& request_object)
{
	Notify(request_object);
}

void Stabilize::HandleNotification(const RequestObject& request_object, HandlerObject<HandlerFunction> handler)
{
	Notify(request_object, handler);
}

void Stabilize::HandleNotification()
{
  const auto MessagePtr = dependencies_.front().first;

  if(MessagePtr == nullptr)
  {
    state_ = MESSAGE_STATE::FAILED;
    *stabilize_flag_ = false;
    RequestDestruction();
    return;
  }

  if(!MessagePtr->is_done() || MessagePtr->get_state() != MESSAGE_STATE::DONE)
  {
    state_ = MESSAGE_STATE::FAILED;
    *stabilize_flag_ = false;
    RequestDestruction();
    return;
  }

  const auto Type = MessagePtr->GetType();

  if(Type == MessageType::GET_PRED_AND_SUCC_LIST)
  {
    handle_get_pred_and_succ_list_notify(MessagePtr.get());
  }
  else if(Type == MessageType::GET_SELF_AND_SUCC_LIST)
  {
    handle_get_self_and_succ_list_notify(MessagePtr.get());
  }
  else if(Type == MessageType::NOTIFICATION)
  {
    handle_notification_notify(MessagePtr.get());
  }
}


void Stabilize::handle_get_pred_and_succ_list_notify(MessagingBase* MessagePtr)
{
  //PredAndSuccList consists of: {predecessor, successor_list}
  auto PredAndSuccList =
      dynamic_cast<GetPredAndSuccList*>(MessagePtr)->get_future().get().value();

  dependencies_.clear();
  time_point_ = std::chrono::system_clock::now() + DURATION;

  if(PredAndSuccList.empty())
  {
    //TODO: handle invalid message
    //TODO: CHange to pop_front()
    routing_table_->pop_front();
    *stabilize_flag_ = false;
    create_notification();
    return;
  }

  Peer self;
  if(not routing_table_->try_get_self(self))
  {
    util::log(warning, "Cant handle stabilize: self not set");
    state_ = MESSAGE_STATE::FAILED;
    *stabilize_flag_ = false;
    RequestDestruction();
    return;
  }

  //get the predecessor and erase him aftwerwards
  auto successors_predecessor = PredAndSuccList.front();
  if(PredAndSuccList.size() > 1)
  {
    PredAndSuccList.erase(PredAndSuccList.begin());
  }

  //get own successor
  Peer successor;
  if(not routing_table_->try_get_successor(successor))
  {
    util::log(warning, "Cant handle stabilize: successor not set");
    state_ = MESSAGE_STATE::FAILED;
    *stabilize_flag_ = false;
    RequestDestruction();
    return;
  }

  //insert the peer at beginning of his succ list to make it our own succ list
  PredAndSuccList.insert(PredAndSuccList.begin(), successor);
  routing_table_->replace_successor_list(PredAndSuccList);


  //check if successors predecessor will be our new successor
  if(util::between(self.get_id(),
                   successors_predecessor.get_id(),
                   successor.get_id()))
  {
    auto GetSelfAndSuccListMessage =
        std::make_shared<GetSelfAndSuccList>(successors_predecessor);
    GetSelfAndSuccListMessage->Attach(this);

    dependencies_.emplace_back(std::make_pair(std::move(GetSelfAndSuccListMessage), true));
    (*dependencies_.front().first)();
  }
  else
  {
    create_notification();
    *stabilize_flag_ = false;
  }
}

void  Stabilize::handle_get_self_and_succ_list_notify(MessagingBase* MessagePtr)
{
  const auto SelfAndSuccList =
      dynamic_cast<GetSelfAndSuccList*>(MessagePtr)->get_future().get().value();

  dependencies_.clear();
  time_point_ = std::chrono::system_clock::now() + DURATION;

  routing_table_->replace_successor_list(SelfAndSuccList);
  *stabilize_flag_ = false;
  create_notification();
}

void Stabilize::handle_notification_notify(MessagingBase* MessagePtr)
{
  state_ = MESSAGE_STATE::DONE;
  RequestDestruction();
}

void Stabilize::create_notification()
{
  auto NotifyMessage = std::make_shared<Notification>(routing_table_);
  NotifyMessage->Attach(this);

  dependencies_.emplace_back(std::make_pair(std::move(NotifyMessage), true));
  (*dependencies_.front().first)();
}

void Stabilize::create_request()
{
  Peer target;
  if(not routing_table_->try_get_successor(target)){
    state_ = MESSAGE_STATE::FAILED;
    *stabilize_flag_ = false;
    RequestDestruction();
    return;
  }

  auto  GetPredAndSuccListMessage = std::make_shared<GetPredAndSuccList>(target);
  GetPredAndSuccListMessage->Attach(this);

  dependencies_.emplace_back(std::make_pair(std::move(GetPredAndSuccListMessage), true));
  (*dependencies_.front().first)();
}

void Stabilize::handle_request()
{

}
void Stabilize::handle_response(RequestObject request_object)
{

}
void Stabilize::handle_failed()
{
  state_ = MESSAGE_STATE::FAILED;
  *stabilize_flag_ = false;
}



} //closing namespace peerpaste::message
