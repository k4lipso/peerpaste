#include <cassert>

#include "peerpaste/messaging_base.hpp"

MessagingBase::MessagingBase(MessageType type)
	: type_(type)
	, time_point_(std::chrono::system_clock::now() + DURATION)
{
}

MessagingBase::MessagingBase(MessagingBase &&other)
	: Observable(std::move(other))
	, type_(std::move(other.type_))
	, handler_object_(std::move(other.handler_object_))
	, request_(std::move(other.request_))
	, dependencies_(std::move(other.dependencies_))
	, time_point_(std::move(other.time_point_))
	, is_request_handler_(other.is_request_handler_.load())
	, is_done_(other.is_done_.load())
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

bool MessagingBase::IsType(const MessageType &type) const noexcept
{
	return type_ == type;
}

void MessagingBase::RequestDestruction()
{
	is_done_ = true;
	Notify();
}

MessageType MessagingBase::GetType() const noexcept
{
	return type_;
}

MESSAGE_STATE MessagingBase::get_state() const noexcept
{
	return state_;
}

std::chrono::time_point<std::chrono::system_clock> MessagingBase::get_timeout() const
{
	return time_point_;
}

MESSAGE_STATE MessagingBase::check_state()
{
	if(is_done_)
	{
		return MESSAGE_STATE::DONE;
	}

	if(is_timed_out())
	{
		util::log(debug, "Message Timed Out");
		util::log(debug, std::to_string(static_cast<int>(type_)));
		handle_failed();
		is_done_ = true;
		return MESSAGE_STATE::TIMEDOUT;
	}

	return MESSAGE_STATE::VALID;
}

bool MessagingBase::is_timed_out()
{
	if(std::chrono::system_clock::now() > time_point_)
	{
		return true;
	}

	bool removed_essential_dependency = false;
	const auto check_time_out = [&removed_essential_dependency](auto &dependency_pair) {
		if(!dependency_pair.first->is_timed_out())
		{
			return false;
		}

		if(dependency_pair.second)
		{
			removed_essential_dependency = true;
		}

		return true;
	};

	dependencies_.erase(std::remove_if(dependencies_.begin(), dependencies_.end(), check_time_out), dependencies_.end());

	return removed_essential_dependency;
}

void MessagingBase::create_handler_object(const std::string &correlation_id, HandlerFunction handler_function)
{
	handler_object_ =
		std::make_unique<HandlerObject<HandlerFunction>>(correlation_id, handler_function, shared_from_this());
}
