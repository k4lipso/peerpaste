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
        : thread_count_(thread_count), acceptor_(m_io_context), m_routingTable(),
          t(m_io_context, boost::asio::chrono::seconds(5)),
          t2(m_io_context, boost::asio::chrono::seconds(2)),
          stabilize_strand_(m_io_context)
    {
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
        m_routingTable = std::make_shared<RoutingTable>(p, nullptr, nullptr);
        m_routingTable->set_successor(p);
        BOOST_LOG_TRIVIAL(info) << "[SERVER] m_self ID: "
                                << m_routingTable->get_self()->getID();
        m_routingTable->print();
    }


    /**
     * Joins a Ring using a known Node inside of that ring
     * that Node will lookup and return a successor Recursively
     * returns -1 when failed
    */
    void join(std::string addr, uint16_t server_port)
    {
        //create endpoint
        tcp::resolver resolver(m_io_context);
        auto endpoint = resolver.resolve(addr, std::to_string(server_port));

        //create session object
        auto join_handler =
            std::make_shared<Session>(m_io_context, m_routingTable);

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
            std::make_shared<Session>(m_io_context, m_routingTable);


        acceptor_.async_accept( handler->socket(),
                                [=] (auto ec)
                                {
                                    handle_new_connection(handler, ec);
                                }
                              );

        for(int i=0; i < thread_count_; ++i)
        {
            thread_pool_.emplace_back( [=]{ m_io_context.run(); } );
        }
    }

    void handle_new_connection(Session::SessionPtr handler,
            const boost::system::error_code& ec)
    {
        BOOST_LOG_TRIVIAL(info) << "[SERVER] handle_accept with error";
        if(ec) { return; }

        BOOST_LOG_TRIVIAL(info) << "[SERVER] handle_accept with no error";
        handler->start();

        auto new_handler = std::make_shared<Session>(m_io_context, m_routingTable);

        acceptor_.async_accept( new_handler->socket(),
                                [=] (auto ec)
                                {
                                    handle_new_connection( new_handler, ec);
                                }
                              );
    }



    void stabilize()
    {
        t.async_wait(boost::bind(&Server::stabilize_internal, this));
    }

    void stabilize_internal()
    {
        std::cout << "stabilize()...." << std::endl;

        auto stabilize_handler =
            std::make_shared<Session>(m_io_context, m_routingTable);

        //TODO: sometimes we get stuck when waiting for stabilize response if peer died!
        //we have to set a timeout mechanism
        /* m_io_context.post(stabilize_strand_.wrap( [=] () */
        /*             { */
                        stabilize_handler->stabilize();
                    /* } )); */
        /* stabilize_handler->stabilize(); */

        //TODO: should wait for stabilize here
        auto notify_handler =
            std::make_shared<Session>(m_io_context, m_routingTable);
        m_io_context.post(stabilize_strand_.wrap( [=] ()
                    {
                        notify_handler->notify();
                    } ));

        //check_predecessor handler
        auto predecessor_handler =
            std::make_shared<Session>(m_io_context, m_routingTable);
        m_io_context.post(stabilize_strand_.wrap( [=] ()
                    {
                        predecessor_handler->check_predecessor();
                    } ));

        t.expires_at(t.expiry() + boost::asio::chrono::milliseconds(1));
        t.async_wait(boost::bind(&Server::stabilize, this));
    }

    void send_routing_information()
    {
        t2.async_wait(boost::bind(&Server::send_routing_information_internal, this));
    }

    void send_routing_information_internal()
    {
        tcp::resolver resolver(m_io_context);
        auto endpoint = resolver.resolve("127.0.0.1", "8080");
        tcp::socket socket(m_io_context);
        boost::asio::connect(socket, endpoint);

        boost::asio::streambuf request;
        std::ostream request_stream(&request);

       using boost::property_tree::ptree;
       using boost::property_tree::read_json;
       using boost::property_tree::write_json;

        ptree root, info;
        if(m_routingTable->get_successor() != nullptr){
            info.put("successor", m_routingTable->get_successor()->getID());
        } else {
            info.put("successor", "");
        }
        if(m_routingTable->get_predecessor() != nullptr){
            info.put("predecessor", m_routingTable->get_predecessor()->getID());
        } else {
            info.put("predecessor", "");
        }
        root.put_child(m_routingTable->get_self()->getID(), info);
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
        t2.expires_at(t.expiry() + boost::asio::chrono::seconds(3));
        t2.async_wait(boost::bind(&Server::send_routing_information_internal, this));

    }

    void notify(/*IP*/);

    void fix_fingers();

    void check_predecessor();


    //TODO:
    //MAKE READs/WRITEs ATOMIC
    //ATOMIC READS AND WRITES
    //Fingertable containing multiple peers,
    //used to lookup keys
    std::shared_ptr<RoutingTable> m_routingTable;

    boost::asio::io_context m_io_context;
    boost::asio::io_service::strand stabilize_strand_;
    boost::asio::steady_timer t;
    boost::asio::steady_timer t2;
    /* tcp::socket m_socket; */
    int thread_count_;
    std::vector<std::thread> thread_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;
    //TODO: timestamp for uptime (part of peerinfo)
};

#endif /* SERVER_H */
