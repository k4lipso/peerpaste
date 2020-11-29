#pragma once

#include "message.hpp"
#include "peer.hpp"
//#include "peerpaste/messaging_base.hpp"
// TODO: change to abstract session
#include <future>
#include <optional>
#include <variant>
#include <functional>

#include "boost_session.hpp"

class RequestObject;
class MessagingBase;

using MessagePtr = std::shared_ptr<Message>;
using PeerPtr = std::shared_ptr<Peer>;
using SessionPtr = std::shared_ptr<Session>;
using RequestObjectUPtr = std::unique_ptr<RequestObject>;
using HandlerFunctionDeprecated = std::function<void(RequestObjectUPtr)>;
using HandlerFunction = std::function<void(RequestObject)>;
using DataPromise = std::shared_ptr<std::promise<std::string>>;

enum class MessageType
{
	UNKNOWN = 0,
	NOTIFICATION,
	CHECK_PREDECESSOR,
	QUERY,
	FIND_SUCCESSOR,
	JOIN,
	GET_SUCCESSOR_LIST,
	GET_SELF_AND_SUCC_LIST,
	GET_PRED_AND_SUCC_LIST,
	STABILIZE,
	BROADCAST_FILELIST,
	GET_FILE,
};

template<typename Handler>
struct HandlerObject
{
	HandlerObject(std::string correlation_id, const Handler &handler, std::weak_ptr<MessagingBase> parent)
		: correlation_id_(std::move(correlation_id))
		, handler_(handler)
		, parent_(parent)
	{
	}

	HandlerObject(std::string correlation_id,
								const Handler &handler,
								std::weak_ptr<MessagingBase> parent,
								bool is_persistent)
		: correlation_id_(std::move(correlation_id))
		, handler_(handler)
		, parent_(parent)
		, is_persistent_(is_persistent)
	{
	}

	HandlerObject(const HandlerObject &other)
		: correlation_id_(other.correlation_id_)
		, handler_(other.handler_)
		, parent_(other.parent_)
		, is_persistent_(other.is_persistent_)
	{
	}

	bool operator==(const std::string &other_id) const
	{
		return correlation_id_ == other_id;
	}

	bool operator<(const HandlerObject<Handler> &other) const
	{
		return correlation_id_ < other.correlation_id_;
	}

	bool is_valid() const
	{
		return !parent_.expired();
	}

	auto lock() const
	{
		return parent_.lock();
	}

	void set_persistency(bool persistency)
	{
		is_persistent_ = persistency;
	}

	bool get_persistency() const
	{
		return is_persistent_;
	}

	bool is_persistent() const
	{
		return is_persistent_;
	}


	std::string correlation_id_;
	Handler handler_;
	std::weak_ptr<MessagingBase> parent_;
	bool is_persistent_ = false;
};

template<typename Handler>
bool operator<(const HandlerObject<Handler> &first, const std::string &second)
{
	return first.correlation_id_ < second;
}

template<typename Handler>
bool operator<(const std::string &first, const HandlerObject<Handler> &second)
{
	return first < second.correlation_id_;
}

class RequestObject
{
public:
	RequestObject(MessageType type = MessageType::UNKNOWN)
		: type_(type)
	{
		start_ = std::chrono::steady_clock::now();
	}

	/*
	 * Calls the optional handlerfunction with the
	 * given request object
	 * The Idea is that you create a Request, set a handler
	 * and store it in an open_requests container
	 * As soon as you get a response with a matching
	 * corelational ID you pass the response object
	 * to this call function of the original request object
	 * which will execute the function with the response as an
	 * argument
	 */
	void call(RequestObjectUPtr request)
	{
		// throw exeption if no value
		if(handler_.has_value())
			handler_.value()(std::move(request));
	}

	void call(std::shared_ptr<RequestObject> request)
	{
		// throw exeption if no value
		auto unique_request = std::make_unique<RequestObject>(*request.get());
		if(handler_.has_value())
			handler_.value()(std::move(unique_request));
	}

	void set_handler(HandlerFunctionDeprecated handler)
	{
		handler_ = handler;
	}

	bool has_handler() const
	{
		return handler_.has_value();
	}

	const HandlerFunctionDeprecated get_handler() const
	{
		return handler_.value();
	}

	void set_promise(const DataPromise &data_promise)
	{
		data_promise_deprecated_ = data_promise;
	}

	bool has_promise() const
	{
		return data_promise_deprecated_.has_value();
	}

	bool set_promise_value(const std::string &value)
	{
		if(not has_promise())
		{
			util::log(debug, "request_object::set_promise_value: has no promise");
			return false;
		}
		data_promise_deprecated_.value()->set_value(value);
		return true;
	}

	void set_message(MessagePtr message)
	{
		message_ = message;
	}

	const MessagePtr get_message() const
	{
		return message_;
	}

	void set_connection(PeerPtr peer)
	{
		connection_ = peer;
	}

	void set_connection(SessionPtr session)
	{
		connection_ = session;
	}

	bool is_session() const
	{
		return std::holds_alternative<SessionPtr>(connection_);
	}

	const SessionPtr get_session() const
	{
		return std::get<SessionPtr>(connection_);
	}

	const PeerPtr get_peer() const
	{
		return std::get<PeerPtr>(connection_);
	}

	const std::string get_correlational_id() const noexcept
	{
		return message_->get_correlational_id();
	}

	const std::string get_request_type() const noexcept
	{
		return message_->get_request_type();
	}

	bool is_request() const
	{
		return message_->is_request();
	}

	const std::string get_client_ip() const
	{
		if(is_session())
		{
			return get_session()->get_client_ip();
		}
		return get_peer()->get_ip();
	}

	void set_time_point()
	{
		start_ = std::chrono::steady_clock::now();
	}

	auto get_time_point()
	{
		return start_;
	}

	auto is_valid()
	{
		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
		auto dur = std::chrono::duration_cast<std::chrono::seconds>(end - start_).count();
		// at startup duration is somehow > 1000 so we filter by the && statement
		// TODO: find why dur is so bit at the beginning
		if(dur > 10)
		{
			return false;
		}
		return true;
	}

	bool has_on_write_handler() const
	{
		return on_write_handler_.has_value();
	}

	void set_on_write_handler(const std::function<void(bool)>& handler)
	{
		on_write_handler_ = handler;
	}

	auto get_on_write_handler() const
	{
		return on_write_handler_.value();
	}

	MessageType type_;

private:
	static void set_connection(RequestObject &req, std::variant<Peer, SessionPtr> connection)
	{
		if(std::holds_alternative<SessionPtr>(connection))
		{
			req.set_connection(std::get<SessionPtr>(connection));
		}
		else
		{
			req.set_connection(std::make_shared<Peer>(std::get<Peer>(connection)));
		}
	}

	MessagePtr message_;
	std::optional<HandlerFunctionDeprecated> handler_;
	std::optional<DataPromise> data_promise_deprecated_; // deprecated
	std::variant<PeerPtr, SessionPtr> connection_;
	std::chrono::steady_clock::time_point start_;
	std::optional<std::function<void(bool)>> on_write_handler_;
};
