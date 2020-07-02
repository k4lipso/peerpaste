#pragma once

#include <functional>
#include <future>

#include "peerpaste/concurrent_request_handler.hpp"
#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/message.hpp"
#include "peerpaste/message_factory.hpp"
#include "peerpaste/messages/broadcast_file_list.hpp"
#include "peerpaste/messages/check_predecessor.hpp"
#include "peerpaste/messages/get_pred_and_succ_list.hpp"
#include "peerpaste/messages/get_self_and_succ_list.hpp"
#include "peerpaste/messages/join.hpp"
#include "peerpaste/messages/notify.hpp"
#include "peerpaste/messages/stabilize.hpp"
#include "peerpaste/messaging_base.hpp"
#include "peerpaste/observer_base.hpp"
#include "peerpaste/request_object.hpp"
#include "peerpaste/storage.hpp"
#include "peerpaste/thread_pool.hpp"

class MessageHandler : public ObserverBase
{
public:
	typedef std::shared_ptr<Message> MessagePtr;
	typedef std::unique_ptr<RequestObject> RequestObjectUPtr;
	typedef std::shared_ptr<RequestObject> RequestObjectSPtr;
	typedef std::shared_ptr<Peer> PeerPtr;

	MessageHandler(short port)
		: routing_table_()
		, static_storage_(nullptr)
		, stabilize_flag_(false)
		, check_predecessor_flag_(false)
		, thread_pool_(0)
		, message_factory_{nullptr}
	{
		// TODO: setup self more accurate
		auto self_ip = "127.0.0.1";
		auto self_port = std::to_string(port);
		auto self_id = util::generate_sha256(self_ip, self_port);
		routing_table_.set_self(Peer(self_id, self_ip, self_port));

		static_storage_ = std::make_unique<StaticStorage>(self_id);
		message_factory_ = std::make_unique<peerpaste::message::MessageFactory>(&routing_table_, static_storage_.get());
	}

	void init(std::shared_ptr<peerpaste::ConcurrentQueue<RequestObject>> queue__)
	{
		send_queue_ = queue__;
		Peer self;
		if(routing_table_.try_get_self(self))
		{
			routing_table_.set_successor(self);
		}
		else
		{
			util::log(error, "MessageHandler could not init. Self was not set");
		}
	}

	virtual void HandleNotification(const RequestObject &request_object) override
	{
		send_queue_->push(request_object);
	}

	virtual void HandleNotification(const RequestObject &request_object, HandlerObject<HandlerFunction> handler) override
	{
		active_handlers_.insert(handler);
		send_queue_->push(request_object);
	}

	virtual void HandleNotification() override
	{
		//active_messages_.clean();
		active_handlers_.erase_if([](auto &Handler) { return !Handler.is_valid(); });
	}

	void run()
	{
		run_thread_.emplace_back([this] { run_chord_internal(); });
		run_thread_.emplace_back([this] { run_paste_internal(); });
	}

	void stop()
	{
		running_ = false;
		for(auto &t : run_thread_)
		{
			t.join();
		}
	}

	void run_chord_internal()
	{
		if(not stabilize_flag_)
		{
			stabilize();
		}

		if(not check_predecessor_flag_)
		{
			check_predecessor();
		}

		active_messages_.clean();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

		if(running_)
		{
			run_chord_internal();
		}
	}

	void run_paste_internal()
	{
		// const auto files = static_storage_->get_files();
		create_request<peerpaste::message::BroadcastFilelist>(&routing_table_, static_storage_.get());

		std::this_thread::sleep_for(std::chrono::milliseconds(2000));

		if(running_)
		{
			run_paste_internal();
		}
	}

	template<typename MessageType, typename... ArgsT>
	void create_request(ArgsT &&... Args)
	{
		std::shared_ptr<MessageType> Message = message_factory_->create_request<MessageType>(std::forward<ArgsT>(Args)...);

		Message->Attach(this);
		thread_pool_.submit(Message);
		active_messages_.push_back(std::move(Message));
	}

	void join(std::string address, std::string port)
	{
		create_request<peerpaste::message::Join>(&routing_table_, std::move(address), std::move(port));
	}

	void check_predecessor()
	{
		create_request<peerpaste::message::CheckPredecessor>(&routing_table_, &check_predecessor_flag_);
	}

	void notify()
	{
		create_request<peerpaste::message::Notification>(&routing_table_);
	}

	// TODO: add stabilization according to "how to make chord correct", using the
	// extended succ list
	void stabilize()
	{
		stabilize_flag_ = true;
		create_request<peerpaste::message::Stabilize>(&routing_table_, &stabilize_flag_);
	}

	void handle_request(RequestObjectUPtr transport_object)
	{
		std::shared_ptr<MessagingBase> message_object = message_factory_->create_from_request(*transport_object);

		if(!message_object)
		{
			util::log(warning, "Handle_request: couldnt create from tranport object");
			return;
		}

		message_object->Attach(this);
		thread_pool_.submit(message_object);
		active_messages_.push_back(std::move(message_object));
	}

	void handle_response(RequestObjectUPtr transport_object)
	{
		// Get the CorrelationID to check if there is an OpenRequest matching
		// that ID
		auto correlational_id = transport_object->get_correlational_id();

		const auto handler_object = active_handlers_.get_and_erase(correlational_id);
		if(handler_object.has_value())
		{
			if(auto parent = handler_object.value().lock())
			{
				handler_object.value().handler_(*transport_object);
			}
			else
			{
				util::log(info, "weak_ptr expired");
			}

			return;
		}
	}

	std::tuple<Peer, Peer, Peer> get_routing_information()
	{
		Peer pre, self, succ;
		routing_table_.try_get_predecessor(pre);
		routing_table_.try_get_self(self);
		routing_table_.try_get_successor(succ);
		return std::make_tuple(pre, self, succ);
	}

	ThreadPool thread_pool_;

private:
	peerpaste::ConcurrentRoutingTable<Peer> routing_table_;
	std::shared_ptr<peerpaste::ConcurrentQueue<RequestObject>> send_queue_;
	std::unique_ptr<StaticStorage> static_storage_;

	mutable std::mutex mutex_;

	std::atomic<bool> stabilize_flag_;
	std::atomic<bool> check_predecessor_flag_;
	bool running_ = true;
	std::vector<std::thread> run_thread_;

	std::unique_ptr<peerpaste::message::MessageFactory> message_factory_;
	peerpaste::ConcurrentDeque<std::shared_ptr<MessagingBase>> active_messages_;
	peerpaste::ConcurrentSet<HandlerObject<HandlerFunction>, std::less<>> active_handlers_;
};
