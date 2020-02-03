#include <cassert>

#include "peerpaste/messaging_base.hpp"

MessagingBase::MessagingBase(MessageType type)
{
  type_ = type;
}

MessagingBase::MessagingBase(MessagingBase&& other)
  : Observable(std::move(other))
  , promise_(std::move(other.promise_))
  , correlational_id_(std::move(other.correlational_id_))
  , dependencies_(std::move(other.dependencies_))
  ,	is_done_(other.is_done_.load())
{
}

MessagingBase::~MessagingBase()
{
}

bool MessagingBase::operator<(const MessagingBase& other) const
{
	assert(correlational_id_.has_value() && other.correlational_id_.has_value());
	return correlational_id_.value() < other.correlational_id_.value();
}

bool MessagingBase::is_done() const noexcept
{
  return is_done_;
}

bool MessagingBase::is_waiting() const
{
  return correlational_id_.has_value();
}

bool MessagingBase::awaits(const std::string& id) const
{
  return correlational_id_.has_value() ? correlational_id_.value() == id : false;
}

std::future<std::string> MessagingBase::get_future()
{
  return promise_.get_future();
}
