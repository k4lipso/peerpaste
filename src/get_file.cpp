#include "peerpaste/messages/get_file.hpp"
namespace peerpaste::message
{

GetFile::GetFile(StaticStorage *storage, Peer target, std::string file_name)
	: MessagingBase(MessageType::GET_FILE)
	, storage_(storage)
	, target_(std::move(target))
	, file_name_(std::move(file_name))
{
}

GetFile::GetFile(StaticStorage *storage, RequestObject request)
	: MessagingBase(MessageType::GET_FILE, std::move(request))
	, storage_(storage)
{
}

GetFile::GetFile(GetFile &&other)
	: MessagingBase(std::move(other))
	, Awaitable(std::move(other))
	, storage_(std::move(other.storage_))
	, target_(std::move(other.target_))
	, file_name_(std::move(other.file_name_))
{
}

GetFile::~GetFile()
{
	// set_promise(state_);
}

void GetFile::HandleNotification(const RequestObject &request_object)
{
}

void GetFile::create_request()
{
	std::scoped_lock lk{mutex_};
	if(!target_.has_value() || !file_name_.has_value())
	{
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	const auto message = std::make_shared<Message>();
	message->set_header(Header(true, 0, 0, "get_file", "", "", ""));
	message->set_data(file_name_.value());

	const auto transaction_id = message->generate_transaction_id();

	const auto handler = std::bind(&GetFile::handle_response, this, std::placeholders::_1);

	auto request = RequestObject();
	request.set_message(message);
	request.set_connection(std::make_shared<Peer>(target_.value()));

	create_handler_object(transaction_id, handler);
	Notify(request, *handler_object_);
}

void GetFile::handle_request()
{
	std::scoped_lock lk{mutex_};
	if(!request_.has_value())
	{
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	auto message = request_.value().get_message();

	const auto file_name = message->get_data();

	std::string data;

	if(storage_->exists(file_name))
	{
		data = storage_->get(file_name);
	}

	auto response = message->generate_response();
	response->set_data(data);

	response->generate_transaction_id();

	auto response_object = RequestObject(request_.value());
	response_object.set_message(response);

	Notify(response_object);
	state_ = MESSAGE_STATE::DONE;
	RequestDestruction();
}

void GetFile::handle_response(RequestObject request_object)
{
	std::scoped_lock lk{mutex_};
	if(request_object.is_request())
	{
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	const auto data = request_object.get_message()->get_data();

	if(data.empty())
	{
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	storage_->put(data, file_name_.value());

	state_ = MESSAGE_STATE::DONE;
	RequestDestruction();
}

void GetFile::handle_failed()
{
	state_ = MESSAGE_STATE::FAILED;
}

} // namespace peerpaste::message
