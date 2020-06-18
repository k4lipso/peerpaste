#pragma once

#include <optional>

#include "peerpaste/messaging_base.hpp"
#include "peerpaste/storage.hpp"

namespace peerpaste::message
{

class GetFile : public MessagingBase, public Awaitable<MESSAGE_STATE>
{
public:
	GetFile(StaticStorage *storage, Peer target, std::string file_name);
	GetFile(StaticStorage *storage, RequestObject request);
	explicit GetFile(GetFile &&other);

	~GetFile() override;

	void HandleNotification(const RequestObject &request_object) override;

private:
	void create_request() override;
	void handle_request() override;
	void handle_response(RequestObject request_object) override;
	void handle_failed() override;

	StaticStorage *storage_;
	std::optional<Peer> target_;
	std::optional<std::string> file_name_;
};

} // namespace peerpaste::message
