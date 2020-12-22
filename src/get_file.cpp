#include "peerpaste/messages/get_file.hpp"
namespace peerpaste::message
{

GetFile::GetFile(StaticStorage *storage, Peer target, peerpaste::FileInfo file_info)
	: MessagingBase(MessageType::GET_FILE)
	, storage_(storage)
	, target_(std::move(target))
	, file_info_(std::move(file_info))
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
	, file_info_(std::move(other.file_info_))
{
}

GetFile::~GetFile()
{
}

void GetFile::HandleNotification(const RequestObject &request_object)
{
}

void GetFile::create_request()
{
	std::scoped_lock lk{mutex_};
	if(!target_.has_value() || !file_info_.has_value())
	{
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	const auto message = std::make_shared<Message>();
	message->set_header(Header(true, 0, 0, "get_file", "", "", ""));
	message->set_data(file_info_.value().file_name);

	const auto transaction_id = message->generate_transaction_id();

	const auto handler = std::bind(&GetFile::handle_response, this, std::placeholders::_1);

	auto output_file = storage_->create_file(file_info_.value().file_name);
	m_file_size = file_info_.value().file_size;

	if(!output_file.has_value())
	{
		util::log(error, "GetFIle::create_request could not open file");
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	m_output_file = std::move(output_file.value());

	auto request = RequestObject();
	request.set_message(message);
	request.set_connection(std::make_shared<Peer>(target_.value()));

	std::cout << "Requesting file: " << file_info_.value().file_name <<  " of size: " << m_file_size << " bytes\n";

	create_handler_object(transaction_id, handler, true);
	Notify(request, *handler_object_);
}

void GetFile::handle_request()
{
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

		if(!storage_->exists(file_name))
		{
			util::log(error, "Cant send file, it does no exists");
			state_ = MESSAGE_STATE::FAILED;
			RequestDestruction();
			return;
		}

		auto source_file = storage_->read_file(file_name);

		if(!source_file.has_value())
		{
			util::log(error, "Cant read file");
			state_ = MESSAGE_STATE::FAILED;
			RequestDestruction();
			return;
		}

		m_source_file = std::move(source_file.value());

		m_source_file.seekg(0, m_source_file.end);
		const auto file_size = m_source_file.tellg();
		m_source_file.seekg(0, m_source_file.beg);

		std::stringstream sstr1;
		sstr1 << "FileSize: " << file_size;
		util::log(debug, sstr1.str());

		std::cout << "Start sending file " << file_name << " of size: " << file_size << " bytes\n";
	}
	write_file(false);
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

	const auto file_chunk = request_object.get_message()->get_file_chunk();

	if(!m_output_file)
	{
		util::log(error, "GetFIle::create_request could not open file");
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	m_output_file.write(file_chunk.value().data.data(), file_chunk.value().size);

	if(m_output_file.tellp() < static_cast<std::streamsize>(m_file_size))
	{
		time_point_ = std::chrono::system_clock::now() + DURATION;
		Notify();
		auto s = request_object.get_session();
		s->read();
		return;
	}

	util::log(info, std::string("received file ") + file_info_.value().file_name);
	util::log(info, std::string("with size in mb: ") + std::to_string(static_cast<double>(m_file_size / 1048576.0)));
	m_output_file.flush();

	state_ = MESSAGE_STATE::DONE;

	if(!storage_->finalize_file(file_info_.value().file_name))
	{
		state_ = MESSAGE_STATE::FAILED;
	}

	RequestDestruction();
}

void GetFile::handle_failed()
{
	state_ = MESSAGE_STATE::FAILED;
}

void GetFile::write_buffer()
{
	time_point_ = std::chrono::system_clock::now() + DURATION;
	auto response = request_->get_message()->generate_response();
	response->set_file_chunk(peerpaste::FileChunk{m_buf.data(), m_source_file.gcount()});
	response->generate_transaction_id();


	auto response_object = RequestObject(request_.value());
	response_object.set_message(response);
	//response_object.set_on_write_handler(std::bind(&GetFile::write_file, this, std::placeholders::_1));
	response_object.set_on_write_handler([this](bool Failed){ this->write_file(Failed); });
	Notify(response_object);
}

void GetFile::write_file(bool failed)
{
	std::scoped_lock lk{mutex_};
	if(!failed && m_source_file)
	{

		m_source_file.read(m_buf.data(), m_buf.size());

		if(m_source_file.fail() && !m_source_file.eof())
		{
			state_ = MESSAGE_STATE::FAILED;
			util::log(debug, "GetFile::handle_request Failed reading file");
			//TODO: send "abort filetransfer message"
			RequestDestruction();
			return;
		}

		std::stringstream sstr;
		sstr << "GetFile::handle_req Sending " << m_source_file.gcount() << "bytes, total: "
				 << m_source_file.tellg() << " bytes.";

		write_buffer();
	}
	else
	{
		state_ = MESSAGE_STATE::FAILED;
		util::log(debug, "GetFile::write_file error occured");
		RequestDestruction();
	}
}

} // namespace peerpaste::message
