#ifndef SERVER_H
#define SERVER_H

#define VERSION "0.0.1"

#include "peerpaste/boost_session.hpp"
#include "peerpaste/concurrent_queue.hpp"
#include "peerpaste/cryptowrapper.hpp"
#include "peerpaste/consumer.hpp"

#include <string>
#include <sstream>
#include <functional>
#include <thread>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using boost::asio::ip::tcp;

class Server
{
public:
    Server(int thread_count, int port,
           std::shared_ptr<peerpaste::ConcurrentQueue<peerpaste::MsgBufPair>> queue__)
        : port_(port),
          thread_count_(thread_count), acceptor_(io_context_),
          queue_(queue__),
          timer_(io_context_, boost::asio::chrono::seconds(1)),
          read_strand_(io_context_),
          write_strand_(io_context_)
    {}

    void run()
    {
        start_listening(port_);
        accept_connections();
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
        util::log(debug, std::string("setting port to ") + std::to_string(port) );
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
        auto handler =
            std::make_shared<BoostSession>(io_context_, queue_);


        acceptor_.async_accept( handler->get_socket(),
                                [=] (auto ec)
                                {
                                    handle_new_connection(handler, ec);
                                }
                              );

        for(int i=0; i < thread_count_; ++i)
        {
            thread_pool_.emplace_back( [=]{ io_context_.run(); } );
        }
    }

    void handle_new_connection(SessionPtr handler,
            const boost::system::error_code& ec)
    {
        if(ec) {
            util::log(error, "handle_accept with error");
            return;
        }

        handler->read();

        auto new_handler = std::make_shared<BoostSession>(io_context_, queue_);

        acceptor_.async_accept( new_handler->get_socket(),
                                [=] (auto ec)
                                {
                                    handle_new_connection( new_handler, ec);
                                }
                              );
    }

    boost::asio::io_context io_context_;
    boost::asio::io_service::strand read_strand_;
    boost::asio::io_service::strand write_strand_;
    int thread_count_;
    std::vector<std::thread> thread_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::steady_timer timer_;

    std::shared_ptr<peerpaste::ConcurrentQueue<peerpaste::MsgBufPair>> queue_;
    const int port_;
};

#endif /* SERVER_H */

