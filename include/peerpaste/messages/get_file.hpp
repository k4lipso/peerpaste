#pragma once

#include <optional>
#include <fstream>
#include <array>

#include "peerpaste/messaging_base.hpp"
#include "peerpaste/storage.hpp"

namespace peerpaste::message
{

class GetFile : public MessagingBase, public Awaitable<MESSAGE_STATE>
{
public:
	GetFile(StaticStorage *storage, Peer target, peerpaste::FileInfo file_name);
	GetFile(StaticStorage *storage, RequestObject request);
	explicit GetFile(GetFile &&other);

	~GetFile() override;

	void HandleNotification(const RequestObject &request_object) override;

private:
	void create_request() override;
	void handle_request() override;
	void handle_response(RequestObject request_object) override;
	void handle_failed() override;

	void write_buffer();
	void write_file(bool failed);


	StaticStorage *storage_;
	std::optional<Peer> target_;
	std::optional<peerpaste::FileInfo> file_info_;
	std::ifstream m_source_file;
	std::ofstream m_output_file;
	static constexpr size_t m_buffer_size = 1024;
	std::array<char, m_buffer_size> m_buf;
	size_t m_file_size = 0;
};

} // namespace peerpaste::message
