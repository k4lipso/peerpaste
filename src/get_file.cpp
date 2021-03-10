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

void GetFile::HandleNotification(const RequestObject &)
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

	const auto transaction_id = message->generate_transaction_id();

	const auto handler = std::bind(&GetFile::handle_response, this, std::placeholders::_1);

	auto output_file = storage_->create_file(file_info_.value());

	message->add_file(file_info_.value());

	m_file_size = file_info_.value().file_size;

	if(!output_file.has_value())
	{
		spdlog::debug("GetFile::create_request could not open file");
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	m_output_file = std::move(output_file.value());

	auto request = RequestObject();
	request.set_message(message);
	request.set_connection(std::make_shared<Peer>(target_.value()));

	spdlog::info("requesting file: {} of size: {} bytes", file_info_.value().file_name, m_file_size);

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

		const auto file_infos = message->get_files();
		const auto file_info = file_infos.front();

		if(!storage_->exists(file_info))
		{
			spdlog::error("Cant send file, it does no exists");
			state_ = MESSAGE_STATE::FAILED;
			RequestDestruction();
			return;
		}

		auto source_file = storage_->read_file(file_info);

		if(!source_file.has_value())
		{
			spdlog::error("Cant read file");
			state_ = MESSAGE_STATE::FAILED;
			RequestDestruction();
			return;
		}

		m_source_file = std::move(source_file.value());

		m_source_file.seekg(0, m_source_file.end);
		const auto file_size = m_source_file.tellg();
		m_source_file.seekg(file_info.offset, m_source_file.beg);

		spdlog::info("Start sending file: {}", file_info.file_name);
		spdlog::debug("FileSize: {}, Sha256sum: {}", file_size, file_infos.front().sha256sum);

	}
	write_file(false);
}

void GetFile::handle_response(RequestObject request_object)
{
	std::scoped_lock lk{mutex_};
	if(request_object.is_request() || !m_output_file.has_value())
	{
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	const auto file_chunk = request_object.get_message()->get_file_chunk();

	if(!m_output_file.value())
	{
		spdlog::error("GetFIle::create_request could not open file");
		state_ = MESSAGE_STATE::FAILED;
		RequestDestruction();
		return;
	}

	m_output_file.value().write(file_chunk.value());

	if(m_output_file.value().tellp() < static_cast<std::streamsize>(m_file_size))
	{
		time_point_ = std::chrono::system_clock::now() + DURATION;
		Notify();
		auto s = request_object.get_session();
		s->read();
		return;
	}

	spdlog::info("received file {}", file_info_.value().file_name);
	spdlog::info("with size in mb: {}", static_cast<double>(m_file_size / 1048576.0));

	m_output_file.value().flush();

	state_ = MESSAGE_STATE::DONE;

	if(!storage_->finalize_file(file_info_.value()))
	{
		state_ = MESSAGE_STATE::FAILED;
	}

	RequestDestruction();
}

void GetFile::handle_failed()
{
	state_ = MESSAGE_STATE::FAILED;
}

void GetFile::write_buffer(size_t offset /* = 0 */)
{
	time_point_ = std::chrono::system_clock::now() + DURATION;
	auto response = request_->get_message()->generate_response();
	response->set_file_chunk(peerpaste::FileChunk{m_buf.data(), static_cast<size_t>(m_source_file.gcount()), offset});
	response->generate_transaction_id();

	auto response_object = RequestObject(request_.value());
	response_object.set_message(response);
	//response_object.set_on_write_handler(std::bind(&GetFile::write_file, this, std::placeholders::_1));

	response_object.set_on_write_handler([weak = weak_from_this()](bool failed)
	{
		if(auto shared = weak.lock())
		{
			const auto casted_shared = std::dynamic_pointer_cast<GetFile>(shared);

			if(casted_shared)
			{
				casted_shared->write_file(failed);
			}
			else
			{
				spdlog::error("Could not cast to GetFile");
			}

			return;
		}
	});

	Notify(response_object);
}

void GetFile::write_file(bool failed)
{
	std::scoped_lock lk{mutex_};

	if(failed)
	{
	  state_ = MESSAGE_STATE::FAILED;
		spdlog::debug("GetFile::write_file error occured");
		RequestDestruction();
	}

	if(m_source_file)
	{
		const size_t offset = m_source_file.tellg();
		m_source_file.read(m_buf.data(), m_buf.size());

		if(m_source_file.fail() && !m_source_file.eof())
		{
			state_ = MESSAGE_STATE::FAILED;
			spdlog::debug("GetFile::handle_request Failed reading file");

			//TODO: send "abort filetransfer message"
			RequestDestruction();
			return;
		}

		write_buffer(offset);
	}
}

} // namespace peerpaste::message
