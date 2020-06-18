#ifndef SERVER_H
#define SERVER_H

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <functional>
#include <sstream>
#include <string>
#include <thread>

#include "peerpaste/boost_session.hpp"
#include "peerpaste/concurrent_queue.hpp"
#include "peerpaste/consumer.hpp"
#include "peerpaste/cryptowrapper.hpp"

using boost::asio::ip::tcp;

class Server
{
public:
	Server(int thread_count, int port, boost::asio::io_context &io_context)
		: port_(port)
		, thread_count_(thread_count)
		, io_context_(io_context)
		, acceptor_(io_context_)
	{
	}

	void run()
	{
		start_listening(port_);
		accept_connections();
	}

	void set_queue(std::shared_ptr<peerpaste::ConcurrentQueue<peerpaste::MsgBufPair>> queue__)
	{
		queue_ = queue__;
	}

	void stop()
	{
		acceptor_.close();
		util::log(debug, "[Server] closed acceptor");
	}

private:
	/**
	 * Starts listening on the given port
	 */
	void start_listening(uint16_t port)
	{
		/* BOOST_LOG_FUNCTION(); */
		util::log(debug, "Setting up endpoint");
		tcp::endpoint endpoint(tcp::v4(), port);
		util::log(debug, std::string("setting port to ") + std::to_string(port));
		acceptor_.open(endpoint.protocol());
		acceptor_.set_option(tcp::acceptor::reuse_address(true));
		acceptor_.bind(endpoint);
		acceptor_.listen();
		util::log(debug, "started listening");
	}

	/**
	 * Creates first handler which will accept incomming connections
	 */
	void accept_connections()
	{
		auto handler = std::make_shared<BoostSession>(io_context_, queue_);

		acceptor_.async_accept(handler->get_socket(), [this, handler](auto ec) { handle_new_connection(handler, ec); });
	}

	void handle_new_connection(SessionPtr handler, const boost::system::error_code &ec)
	{
		if(ec)
		{
			util::log(error, "handle_accept with error");
			return;
		}
		if(!acceptor_.is_open())
		{
			return;
		}

		handler->read();

		auto new_handler = std::make_shared<BoostSession>(io_context_, queue_);

		acceptor_.async_accept(new_handler->get_socket(),
													 [this, new_handler](auto ec) { handle_new_connection(new_handler, ec); });
	}

	boost::asio::io_context &io_context_;
	int thread_count_;
	std::vector<std::thread> thread_pool_;
	boost::asio::ip::tcp::acceptor acceptor_;

	std::shared_ptr<peerpaste::ConcurrentQueue<peerpaste::MsgBufPair>> queue_;
	const int port_;
};

#endif /* SERVER_H */
