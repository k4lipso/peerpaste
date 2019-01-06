#ifndef SERVER_H
#define SERVER_H

#define VERSION "0.0.1"

#include "cryptowrapper.hpp"
#include "routingTable.hpp"
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
        : thread_count_(thread_count), acceptor_(io_context_),
          timer1_(io_context_, boost::asio::chrono::seconds(5)),
          timer2_(io_context_, boost::asio::chrono::seconds(2)),
          stabilize_strand_(io_context_)
    {
        routing_table_ = RoutingTable::getInstance();
    }

    void start_server(uint16_t port)
    {
        start_listening(port);
        init_routingtable(std::to_string(port));
        accept_connections();
        stabilize();
        send_routing_information();
    }

    void start_client(std::string addr, uint16_t server_port, uint16_t own_port)
    {
        start_listening(own_port);
        init_routingtable(std::to_string(own_port));
        join(addr, server_port);
        accept_connections();
        stabilize();
        send_routing_information();
    }

private:

    /**
     * Creates on Peer Object based on the own IP
     */
    Peer createPeer(std::string port)
    {
        auto endpoint = acceptor_.local_endpoint();
        const boost::asio::ip::address ip_ = endpoint.address();
        const std::string ip = "127.0.0.1";
        const std::string id = util::generate_sha256(ip, port);
        BOOST_LOG_TRIVIAL(info) << "[SERVER] IP: " << ip;
        BOOST_LOG_TRIVIAL(info) << "[SERVER] ID: " << id;
        return Peer(id, ip, port);
    }

    void init_routingtable(std::string port)
    {
        auto p = std::make_shared<Peer>(createPeer(port));
        routing_table_->set_self(p);
        routing_table_->set_successor(p);
        routing_table_->set_predecessor(nullptr);
        /* routing_table_ = std::make_shared<RoutingTable>(p, nullptr, nullptr); */
        /* routing_table_->set_successor(p); */
        BOOST_LOG_TRIVIAL(info) << "[SERVER] m_self ID: "
                                << routing_table_->get_self()->get_id();
        routing_table_->print();
    }


    /**
     * Joins a Ring using a known Node inside of that ring
     * that Node will lookup and return a successor Recursively
     * returns -1 when failed
    */
    void join(std::string addr, uint16_t server_port)
    {
        //create endpoint
        tcp::resolver resolver(io_context_);
        auto endpoint = resolver.resolve(addr, std::to_string(server_port));

        //create session object
        auto join_handler =
            std::make_shared<Session>(io_context_);

        //join the network using the given endpoint
        join_handler->join(endpoint);
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


        acceptor_.async_accept( handler->socket(),
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
        BOOST_LOG_TRIVIAL(info) << "[SERVER] handle_accept with error";
        if(ec) { return; }

        BOOST_LOG_TRIVIAL(info) << "[SERVER] handle_accept with no error";
        handler->start();

        auto new_handler = std::make_shared<Session>(io_context_);

        acceptor_.async_accept( new_handler->socket(),
                                [=] (auto ec)
                                {
                                    handle_new_connection( new_handler, ec);
                                }
                              );
    }



    void stabilize()
    {
        timer1_.async_wait(boost::bind(&Server::stabilize_internal, this));
    }

    void stabilize_internal()
    {
        std::cout << "stabilize()...." << std::endl;

        auto stabilize_handler =
            std::make_shared<Session>(io_context_);

        //TODO: sometimes we get stuck when waiting for stabilize response if peer died!
        //we have to set a timeout mechanism
        /* io_context_.post(stabilize_strand_.wrap( [=] () */
        /*             { */
                        stabilize_handler->stabilize();
                    /* } )); */
        /* stabilize_handler->stabilize(); */

        //TODO: should wait for stabilize here
        auto notify_handler =
            std::make_shared<Session>(io_context_);
        io_context_.post(stabilize_strand_.wrap( [=] ()
                    {
                        notify_handler->notify();
                    } ));

        //check_predecessor handler
        auto predecessor_handler =
            std::make_shared<Session>(io_context_);
        io_context_.post(stabilize_strand_.wrap( [=] ()
                    {
                        predecessor_handler->check_predecessor();
                    } ));

        timer1_.expires_at(timer1_.expiry() + boost::asio::chrono::milliseconds(600));
        timer1_.async_wait(boost::bind(&Server::stabilize, this));
    }

    void send_routing_information()
    {
        timer2_.async_wait(boost::bind(&Server::send_routing_information_internal, this));
    }

    void send_routing_information_internal()
    {
        tcp::resolver resolver(io_context_);
        auto endpoint = resolver.resolve("127.0.0.1", "8080");
        tcp::socket socket(io_context_);
        boost::asio::connect(socket, endpoint);

        boost::asio::streambuf request;
        std::ostream request_stream(&request);

       using boost::property_tree::ptree;
       using boost::property_tree::read_json;
       using boost::property_tree::write_json;

        ptree root, info;
        if(routing_table_->get_successor() != nullptr){
            info.put("successor", routing_table_->get_successor()->get_id());
        } else {
            info.put("successor", "");
        }
        if(routing_table_->get_predecessor() != nullptr){
            info.put("predecessor", routing_table_->get_predecessor()->get_id());
        } else {
            info.put("predecessor", "");
        }
        root.put_child(routing_table_->get_self()->get_id(), info);
        /* root.put ("some value", "8"); */
        /* root.put ( "message", "value value: value!"); */
        /* info.put("placeholder", "value"); */
        /* info.put("value", "daf!"); */
        /* info.put("module", "value"); */
        /* root.put_child("exception", info); */

        std::ostringstream buf;
        write_json (buf, root, false);
        std::string json = buf.str();

        request_stream << "POST / HTTP/1.1 \r\n";
        request_stream << "Host:" << "lol" << "\r\n";
        request_stream << "User-Agent: C/1.0";
        request_stream << "Content-Type: application/json; charset=utf-8 \r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "Content-Length: " << json.length() << "\r\n";
        request_stream << "Connection: close\r\n\r\n";  //NOTE THE Double line feed
        request_stream << json;

        // Send the request.
        boost::asio::write(socket, request);
        std::cout << "written post req" << std::endl;
        timer2_.expires_at(timer2_.expiry() + boost::asio::chrono::seconds(3));
        timer2_.async_wait(boost::bind(&Server::send_routing_information_internal, this));

    }

    void fix_fingers();

    std::shared_ptr<RoutingTable> routing_table_;

    boost::asio::io_context io_context_;
    boost::asio::io_service::strand stabilize_strand_;
    boost::asio::steady_timer timer1_;
    boost::asio::steady_timer timer2_;

    int thread_count_;
    std::vector<std::thread> thread_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;
    //TODO: timestamp for uptime (part of peerinfo)
};

#endif /* SERVER_H */
