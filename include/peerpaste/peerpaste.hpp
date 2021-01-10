#ifndef PEERPASTE
#define PEERPASTE

#include <memory>
#include <string>
#include <thread>

#include "peerpaste/cryptowrapper.hpp"
#include "peerpaste/server.hpp"

namespace peerpaste
{

class PeerPaste
{
public:
	~PeerPaste()
	{
		stop();
	}

	void init(const std::string& ip, unsigned port, size_t thread_count)
	{
		handler_ = std::make_shared<MessageHandler>(ip, port);
		dispatcher_ = std::make_unique<peerpaste::MessageDispatcher>(handler_);
		server_ = std::make_unique<Server>(thread_count, port, dispatcher_->get_context());
		server_->set_queue(dispatcher_->get_receive_queue());
		handler_->init(dispatcher_->get_send_queue());
	}

	void run()
	{
		if(joined_ or create_ring_)
		{
			handler_->run();
		}
		// TODO: server has to run even if for put/get. that means that on a put/get
		// the
		//      responing node is somehow creating a new tcp session which gets not
		//      accepted when server is not listening. that is bad because even for
		//      simple put/get a port forwarding for hosts behind nat would be
		//      needed. this must change
		server_->run();
		dispatcher_->run();
	}

	void stop()
	{
		server_->stop();
		handler_->stop();
		dispatcher_->stop();
	}

	void join(const std::string &ip, const std::string &port)
	{
		handler_->join(ip, port);
		joined_ = true;
	}

	void create_ring()
	{
		// TODO: on ring creation special attributes for the ring should be set.
		// they should be then sent in every message and allways checked so that
		// every peer is aware of them and has the same values set.
		// those values should be: maximum file size, minimum version,
		//                        redundancy, (authentification for private rings)
		create_ring_ = true;
	}

	void debug()
	{
		dispatcher_->send_routing_information();
	}

	void wait_till_finish()
	{
		dispatcher_->join();
	}

private:
	bool joined_ = false;
	bool create_ring_ = false;
	std::unique_ptr<Server> server_ = nullptr;
	std::unique_ptr<peerpaste::MessageDispatcher> dispatcher_ = nullptr;
	std::shared_ptr<MessageHandler> handler_ = nullptr;
};

} // namespace peerpaste

#endif /* ifndef PEERPASTE */
