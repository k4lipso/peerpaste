#ifndef SERVER_H
#define SERVER_H

#define VERSION "0.0.1"

#include "cryptowrapper.hpp"
#include "session.hpp"
#include "peer.hpp"
#include "message.hpp"
#include "proto/messages.pb.h"

#include <string>
#include <sstream>
#include <functional>
#include <thread>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>
#include <sstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using boost::asio::ip::tcp;

class Server
{
public:
    Server(int thread_count = 4, short port = 1337)
        : thread_count_(thread_count), acceptor_(io_context_)
    {
    }

    void start_server(uint16_t port)
    {
        start_listening(port);
        accept_connections();
    }
private:
    /**
     * Starts listening on the given port
     */
    void start_listening(uint16_t port)
    {
        BOOST_LOG_TRIVIAL(info) << "[SERVER] setting up endpoint...";
        tcp::endpoint endpoint(tcp::v4(), port);
        BOOST_LOG_TRIVIAL(info) << "[SERVER] setting port to " << port;
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        BOOST_LOG_TRIVIAL(info) << "[SERVER] started listening...";
    }

    /**
     * Creates first handler which will accept incomming connections
     */
    void accept_connections()
    {
        auto handler =
            std::make_shared<Session>(io_context_);


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

    void handle_new_connection(Session::SessionPtr handler,
            const boost::system::error_code& ec)
    {
        if(ec) {
            BOOST_LOG_TRIVIAL(info) << "[SERVER] handle_accept with error";
            return;
        }

        BOOST_LOG_TRIVIAL(info) << "[SERVER] handle_accept with no error";
        handler->read();

        auto new_handler = std::make_shared<Session>(io_context_);

        acceptor_.async_accept( new_handler->get_socket(),
                                [=] (auto ec)
                                {
                                    handle_new_connection( new_handler, ec);
                                }
                              );
    }

    boost::asio::io_context io_context_;
    int thread_count_;
    std::vector<std::thread> thread_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;
};

#endif /* SERVER_H */

