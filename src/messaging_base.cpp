#include "peerpaste/messaging_base.hpp"

MessagingBase::MessagingBase(MessageType type)
{
  type_ = type;
}

MessagingBase::MessagingBase(MessagingBase&& other)
  : Observable(std::move(other))
  , promise_(std::move(other.promise_))
  , dependencies_(std::move(other.dependencies_))
  ,	is_done_(other.is_done_.load())
{
}

MessagingBase::~MessagingBase()
{
}

bool MessagingBase::is_done() const noexcept
{
  return is_done_;
}

std::future<std::string> MessagingBase::get_future()
{
  return promise_.get_future();
}
