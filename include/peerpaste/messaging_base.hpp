#pragma once

#include <atomic>
#include <future>
#include <optional>
#include <string>
#include <vector>

#include "peerpaste/observable.hpp"
#include "peerpaste/observer_base.hpp"
#include "peerpaste/request_object.hpp"

using HandlerFunction = std::function<void(RequestObject)>;

enum class MESSAGE_STATE : uint8_t
{
	FAILED = 0,
	TIMEDOUT,
	VALID,
	DONE,
};

template<typename ReturnType>
class Awaitable
{
public:
	std::future<ReturnType> get_future()
	{
		return promise_.get_future();
	}

protected:
	void set_promise(ReturnType value)
	{
		promise_.set_value(std::move(value));
	}

	std::promise<ReturnType> promise_;
};

// template<class FutureType = MESSAGE_STATE>
class MessagingBase : public Observable, public ObserverBase, public std::enable_shared_from_this<MessagingBase>
{
public:
	MessagingBase(MessageType type);
	MessagingBase(MessageType type, RequestObject request);
	MessagingBase(MessagingBase &&other);

	virtual ~MessagingBase();

	virtual void operator()();

	bool is_done() const noexcept;
	bool is_request() const noexcept;
	bool IsType(const MessageType &type) const noexcept;
	void RequestDestruction();
	MessageType GetType() const noexcept;
	MESSAGE_STATE get_state() const noexcept;

	std::chrono::time_point<std::chrono::system_clock> get_timeout() const;

	MESSAGE_STATE check_state();
	bool is_timed_out();

protected:
	virtual void create_handler_object(const std::string &correlation_id, HandlerFunction handler_function, bool is_persistent = false);

	virtual void create_request() = 0;
	virtual void handle_request() = 0;
	virtual void handle_response(RequestObject request_object) = 0;

	virtual void handle_failed() = 0;

	// Only root object needs promise. leafobjects do not.
	MessageType type_;
	std::unique_ptr<HandlerObject<HandlerFunction>> handler_object_ = nullptr;
	std::optional<RequestObject> request_;
	std::vector<std::pair<std::shared_ptr<MessagingBase>, bool>> dependencies_;
	std::chrono::time_point<std::chrono::system_clock> time_point_;
	std::atomic<bool> is_done_ = false;
	std::atomic<MESSAGE_STATE> state_ = MESSAGE_STATE::VALID;
	std::atomic<bool> is_request_handler_ = false;
	mutable std::mutex mutex_;
	static constexpr std::chrono::milliseconds DURATION{10000};
};
