#ifndef SERVER_H
#define SERVER_H

#define VERSION "0.0.1"

#include "peerpaste/session.hpp"
#include "peerpaste/peer.hpp"
#include "peerpaste/message.hpp"
#include "peerpaste/message_handler.hpp"
#include "peerpaste/request_object.hpp"
#include "peerpaste/consumer.hpp"
#include "peerpaste/concurrent_queue.hpp"
#include "peerpaste/proto/messages.pb.h"

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
    Server(int thread_count, int port,
           std::shared_ptr<peerpaste::ConcurrentQueue<peerpaste::MsgBufPair>> queue__)
        : port_(port),
          thread_count_(thread_count), acceptor_(io_context_),
          queue_(queue__),
          timer_(io_context_, boost::asio::chrono::seconds(1)),
          read_strand_(io_context_),
          write_strand_(io_context_),
          send_routing_information_(false)
    {
        write_queue_ = WriteQueue::GetInstance();
    }

    void start_server()
    {
        start_listening(port_);
        accept_connections();
        run();
    }

    void start_client(std::string addr, uint16_t server_port, uint16_t own_port)
    {
        start_listening(port_);
        accept_connections();
        run();
    }

    void send_routing_information(const bool b){
        send_routing_information_ = b;
    }

private:

    void run()
    {
        /* message_handler_.handle_timeouts(); */
        /* message_handler_.handle_message(); */
        /* message_handler_.run(); */
        /* message_handler_.stabilize(); */
        /* message_handler_.check_predecessor(); */
        /* message_handler_.notify(); */
        /* message_handler_->run(); */
        /* handle_write_queue(); */
        timer_.expires_at(timer_.expiry() + boost::asio::chrono::milliseconds(200));
        timer_.async_wait(boost::bind(&Server::run, this));
    }

    //TODO: using move() for request?
    void handle_write_queue()
    {
        if(send_routing_information_){
            send_routing_information_internal();
        }

        bool write_queue_is_empty = write_queue_->empty();
        if(write_queue_is_empty){
            return;
        }

        //TODO: store as ptr in write_queue!
        auto request = write_queue_->front();
        write_queue_->pop_front();
        auto message = request->get_message();
        auto is_request = message->is_request();

        if(request->is_session()){
            auto session = request->get_session();
            /* session->write(message); */
            if(is_request){
                session->read();
            }
        } else {
            //TODO: this is too session/boost specific
            //when using a test_socket or something like that this code
            //should be more abstract so that the message_handler does not need
            //to know what kind of object sends the data somewhere
            auto peer = request->get_peer();
            auto write_handler = std::make_shared<Session>(io_context_, queue_);
            /* write_handler->write_to(message, peer->get_ip(), peer->get_port()); */
            if(is_request){
                write_handler->read();
            }
        }

        request->set_time_point();
        handle_write_queue();
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
            std::make_shared<Session>(io_context_, queue_);


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

        handler->read();

        auto new_handler = std::make_shared<Session>(io_context_, queue_);

        acceptor_.async_accept( new_handler->get_socket(),
                                [=] (auto ec)
                                {
                                    handle_new_connection( new_handler, ec);
                                }
                              );
    }

    void send_routing_information_internal()
    {
        /* auto routing_table_ = message_handler_->get_routing_table(); */
        /* tcp::resolver resolver(io_context_); */
        /* auto endpoint = resolver.resolve("127.0.0.1", "8080"); */
        /* tcp::socket socket(io_context_); */
        /* boost::asio::connect(socket, endpoint); */

        /* boost::asio::streambuf request; */
        /* std::ostream request_stream(&request); */

        /* using boost::property_tree::ptree; */
        /* using boost::property_tree::read_json; */
        /* using boost::property_tree::write_json; */

        /* ptree root, info; */
        /* if(routing_table_.get_successor() != nullptr){ */
        /*     info.put("successor", routing_table_.get_successor()->get_id()); */
        /* } else { */
        /*     info.put("successor", ""); */
        /* } */
        /* if(routing_table_.get_predecessor() != nullptr){ */
        /*     info.put("predecessor", routing_table_.get_predecessor()->get_id()); */
        /* } else { */
        /*     info.put("predecessor", ""); */
        /* } */
        /* root.put_child(routing_table_.get_self()->get_id(), info); */
        /* /1* root.put ("some value", "8"); *1/ */
        /* /1* root.put ( "message", "value value: value!"); *1/ */
        /* /1* info.put("placeholder", "value"); *1/ */
        /* /1* info.put("value", "daf!"); *1/ */
        /* /1* info.put("module", "value"); *1/ */
        /* /1* root.put_child("exception", info); *1/ */

        /* std::ostringstream buf; */
        /* write_json (buf, root, false); */
        /* std::string json = buf.str(); */

        /* request_stream << "POST / HTTP/1.1 \r\n"; */
        /* request_stream << "Host:" << "lol" << "\r\n"; */
        /* request_stream << "User-Agent: C/1.0"; */
        /* request_stream << "Content-Type: application/json; charset=utf-8 \r\n"; */
        /* request_stream << "Accept: *1/*\r\n"; */
        /* request_stream << "Content-Length: " << json.length() << "\r\n"; */
        /* request_stream << "Connection: close\r\n\r\n";  //NOTE THE Double line feed */
        /* request_stream << json; */

        /* // Send the request. */
        /* boost::asio::write(socket, request); */
    }

    boost::asio::io_context io_context_;
    boost::asio::io_service::strand read_strand_;
    boost::asio::io_service::strand write_strand_;
    int thread_count_;
    std::vector<std::thread> thread_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::steady_timer timer_;

    std::shared_ptr<WriteQueue> write_queue_;
    std::shared_ptr<peerpaste::ConcurrentQueue<peerpaste::MsgBufPair>> queue_;

    const int port_;
    bool send_routing_information_;
};

#endif /* SERVER_H */

