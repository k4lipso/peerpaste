#ifndef SERVER_H
#define SERVER_H

#define VERSION "0.0.1"

#include "session.hpp"
#include "peer.hpp"
#include "message.hpp"
#include "message_handler.hpp"
#include "request_object.hpp"
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
        : thread_count_(thread_count), acceptor_(io_context_),
          message_handler_(io_context_, port),
          timer_(io_context_, boost::asio::chrono::seconds(1))
    {
        write_queue_ = WriteQueue::GetInstance();
    }

    void start_server(uint16_t port)
    {
        start_listening(port);
        accept_connections();
        message_handler_.init();
        run();
    }

    void start_client(std::string addr, uint16_t server_port, uint16_t own_port)
    {
        start_listening(own_port);
        accept_connections();
        message_handler_.join(addr, std::to_string(server_port));
        run();
    }
private:

    void run()
    {
        message_handler_.handle_message();
        message_handler_.stabilize();
        message_handler_.notify();
        handle_write_queue();

        timer_.expires_at(timer_.expiry() + boost::asio::chrono::seconds(1));
        timer_.async_wait(boost::bind(&Server::run, this));

    }

    //TODO: using move() for request?
    void handle_write_queue()
    {
        bool write_queue_is_empty = write_queue_->empty();
        if(write_queue_is_empty){
            return;
        }

        //TODO: store as ptr in write_queue!
        auto request = std::make_shared<RequestObject>(*write_queue_->front());
        write_queue_->pop_front();
        auto message = request->get_message();
        auto is_request = message->is_request();

        if(request->is_session()){
            auto session = request->get_session();
            session->write(message);
            if(is_request){
                session->read();
            }
        } else {
            //TODO: this is to session/boost specific
            //when using a test_socket or something like that this code
            //should be more abstract so that the message_handler does not need
            //to know what kind of object sends the data somewhere
            auto peer = request->get_peer();
            auto write_handler = std::make_shared<Session>(io_context_);
            write_handler->write_to(message, peer->get_ip(), peer->get_port());
            if(is_request){
                write_handler->read();
            }
        }
    }


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
    boost::asio::steady_timer timer_;

    std::shared_ptr<WriteQueue> write_queue_;
    MessageHandler message_handler_;
};

#endif /* SERVER_H */

