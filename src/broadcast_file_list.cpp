#include "peerpaste/messages/broadcast_file_list.hpp"
#include "peerpaste/messages/get_file.hpp"

namespace peerpaste::message
{

BroadcastFilelist::BroadcastFilelist(ConcurrentRoutingTable<Peer> *routing_table, StaticStorage *storage)
	: MessagingBase(MessageType::BROADCAST_FILELIST)
	, routing_table_(routing_table)
	, storage_(storage)
{
}

BroadcastFilelist::BroadcastFilelist(ConcurrentRoutingTable<Peer> *routing_table,
																		 StaticStorage *storage,
																		 RequestObject request)
	: MessagingBase(MessageType::BROADCAST_FILELIST, request)
	, routing_table_(routing_table)
	, storage_(storage)
{
}

BroadcastFilelist::BroadcastFilelist(BroadcastFilelist &&other)
	: MessagingBase(std::move(other))
	, Awaitable(std::move(other))
	, routing_table_(std::move(other.routing_table_))
	, storage_(std::move(other.storage_))
{
}

BroadcastFilelist::~BroadcastFilelist()
{
}

void BroadcastFilelist::HandleNotification(const RequestObject &request_object, HandlerObject<HandlerFunction> handler)
{
	time_point_ = std::chrono::system_clock::now() + DURATION;

	Notify(request_object, handler);
}

void BroadcastFilelist::HandleNotification()
{
	time_point_ = std::chrono::system_clock::now() + DURATION;
	if(flag_ && std::all_of(dependencies_.begin(), dependencies_.end(), [](auto dep) { return dep.first->is_done(); }))
	{
		state_ = MESSAGE_STATE::DONE;
		RequestDestruction();
	}
}

void BroadcastFilelist::create_request()
{
	std::scoped_lock lk{mutex_};
	Peer self;

	if(not routing_table_->try_get_self(self))
	{
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	Peer target;
	if(not routing_table_->try_get_successor(target))
	{
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	const auto message = std::make_shared<Message>(Message::create_request("broadcast_filelist", {self}));

	message->set_filelist(storage_->get_files());

	const auto transaction_id = message->get_transaction_id();

	// const auto handler = std::bind(&BroadcastFilelist::handle_response, this, std::placeholders::_1);

	RequestObject request{type_};
	request.set_message(message);
	request.set_connection(std::make_shared<Peer>(target));

	// create_handler_object(transaction_id, handler);
	Notify(request);
	state_ = MESSAGE_STATE::DONE;
	RequestDestruction();
	// Notify(request, *handler_object_);
}

void BroadcastFilelist::handle_request()
{
	std::scoped_lock lk{mutex_};
	if(!request_.has_value())
	{
		state_ = MESSAGE_STATE::FAILED;
		return;
	}

	const auto message = request_.value().get_message();

	const auto file_list = message->get_files();

	Peer self;
	if(not routing_table_->try_get_self(self))
	{
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	auto peers = message->get_peers();

	for(const auto &peer : peers)
	{
		if(peer.get_ip() == self.get_ip() && peer.get_port() == self.get_port())
		{
			// we allready got this message, so we just stop here
			state_ = MESSAGE_STATE::DONE;
			RequestDestruction();
			return;
		}
	}

	peers.push_back(self);

	flag_ = false;
	for(const auto &file : file_list)
	{
		if(!storage_->exists(file.file_name))
		{
			auto get_file_message = std::make_shared<GetFile>(storage_, peers.front(), file);
			get_file_message->Attach(weak_from_this());

			dependencies_.emplace_back(std::make_pair(get_file_message, false));
			(*get_file_message.get())();
		}
	}
	flag_ = true;

	Peer target;
	if(not routing_table_->try_get_successor(target))
	{
		state_ = MESSAGE_STATE::FAILED;

		if(dependencies_.empty())
		{
			RequestDestruction();
		}

		return;
	}

	const auto new_message = std::make_shared<Message>(Message::create_request("broadcast_filelist", peers));

	new_message->set_filelist(file_list);

	// const auto handler = std::bind(&BroadcastFilelist::handle_response, this, std::placeholders::_1);

	RequestObject request{type_};
	request.set_message(new_message);
	request.set_connection(std::make_shared<Peer>(target));

	// create_handler_object(transaction_id, handler);
	Notify(request);
	state_ = MESSAGE_STATE::DONE;

	if(dependencies_.empty())
	{
		RequestDestruction();
	}
}

void BroadcastFilelist::handle_response(RequestObject request_object)
{
}

void BroadcastFilelist::handle_failed()
{
	state_ = MESSAGE_STATE::FAILED;
}

void BroadcastFilelist::HandleNotification(const RequestObject &request_object)
{
}

} // namespace peerpaste::message
