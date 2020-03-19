#include <cassert>

#include "peerpaste/messaging_base.hpp"

MessagingBase::MessagingBase(MessageType type)
  : type_(type)
  , time_point_(std::chrono::system_clock::now() + DURATION)
{
}

MessagingBase::MessagingBase(MessagingBase&& other)
  : Observable(std::move(other))
  , type_(std::move(other.type_))
  , promise_(std::move(other.promise_))
  , handler_object_(std::move(other.handler_object_))
  , request_(std::move(other.request_))
  , dependencies_(std::move(other.dependencies_))
  , time_point_(std::move(other.time_point_))
  , is_request_handler_(other.is_request_handler_.load())
  ,	is_done_(other.is_done_.load())
{
}

MessagingBase::MessagingBase(MessageType type, RequestObject request)
	: type_(type)
	, request_(request)
	, time_point_(std::chrono::system_clock::now() + DURATION)
	, is_request_handler_(true)
{
}


MessagingBase::~MessagingBase()
{
}

void MessagingBase::operator()()
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

bool MessagingBase::is_done() const noexcept
{
  return is_done_;
}

bool MessagingBase::is_request() const noexcept
{
  return is_request_handler_;
}

bool MessagingBase::IsType(const MessageType& type) const noexcept
{
  return type_ == type;
}

MessageType MessagingBase::GetType() const noexcept
{
  return type_;
}

std::future<std::string> MessagingBase::get_future()
{
  return promise_.get_future();
}

std::chrono::time_point<std::chrono::system_clock> MessagingBase::get_timeout() const
{
  return time_point_;
}

void MessagingBase::create_handler_object(const std::string& correlation_id, HandlerFunction handler_function)
{
  handler_object_ = std::make_unique<HandlerObject<HandlerFunction>>(correlation_id, handler_function, shared_from_this());
}
