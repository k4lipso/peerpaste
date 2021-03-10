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
	, is_done_(other.is_done_.load())
	, is_request_handler_(other.is_request_handler_.load())
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
	std::scoped_lock lk{mutex_};
	if(is_done_)
	{
		return state_;
	}

	if(std::chrono::system_clock::now() > time_point_)
	{
		spdlog::debug("Message Timed Out, type: {}", static_cast<int>(type_));
		state_ = MESSAGE_STATE::TIMEDOUT;
		is_done_ = true;
		handle_failed();
		return state_;
	}

	const auto already_timed_out = [](auto dependencie_pair) {
		const auto state = dependencie_pair.first->check_state();
		if(state == MESSAGE_STATE::FAILED || state == MESSAGE_STATE::TIMEDOUT)
		{
			return dependencie_pair.second;
		}
		return false;
	};

	if(std::any_of(dependencies_.begin(), dependencies_.end(), already_timed_out))
	{
		spdlog::debug("Message Timed Out, type: {}", static_cast<int>(type_));
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

	std::for_each(dependencies_.begin(), dependencies_.end(), check_time_out);

	return removed_essential_dependency;
}

void MessagingBase::create_handler_object(const std::string &correlation_id, HandlerFunction handler_function, bool is_persistent /* = false */)
{
	handler_object_ =
		std::make_unique<HandlerObject<HandlerFunction>>(correlation_id, handler_function, shared_from_this(), is_persistent);
}
