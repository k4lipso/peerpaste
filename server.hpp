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

using boost::asio::ip::tcp;

class server
{
public:
    server(int thread_count = 4, short port = 1337)
        : thread_count_(thread_count), acceptor_(m_io_context), m_routingTable()
    {}

    void start_server(uint16_t port)
    {
        start_listening(port);
        init_routingtable();
        accept_connections();
    }

    void start_client(std::string addr, uint16_t server_port, uint16_t own_port)
    {
        start_listening(own_port);
        init_routingtable();
        join(addr, server_port);
        accept_connections();
    }

private:

    /**
     * Creates on Peer Object based on the own IP
     */
    Peer createPeer()
    {
        auto endpoint = acceptor_.local_endpoint();
        const boost::asio::ip::address ip_ = endpoint.address();
        const std::string ip = ip_.to_string();
        const std::string id = util::generate_sha256(ip);
        BOOST_LOG_TRIVIAL(info) << "[SERVER] IP: " << ip;
        BOOST_LOG_TRIVIAL(info) << "[SERVER] ID: " << id;
        return Peer(id, ip);
    }

    void init_routingtable()
    {
        auto p = std::make_shared<Peer>(createPeer());
        m_routingTable = std::make_shared<RoutingTable>(p, nullptr, nullptr);
        BOOST_LOG_TRIVIAL(info) << "[SERVER] m_self ID: "
                                << m_routingTable->get_self()->getID();
    }


    /**
     * Joins a Ring using a known Node inside of that ring
     * that Node will lookup and return a successor Recursively
     * returns -1 when failed
    */
    bool join(std::string addr, uint16_t server_port)
    {
        //create endpoint
        tcp::resolver resolver(m_io_context);
        auto endpoint = resolver.resolve(addr, std::to_string(server_port));

        //create session object
        auto join_handler =
            std::make_shared<session>(m_io_context, m_routingTable);

        //join the network using the given endpoint
        join_handler->join(endpoint);

        return false;
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
            std::make_shared<session>(m_io_context, m_routingTable);


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

    void handle_new_connection(session::SessionPtr handler,
            const boost::system::error_code& ec)
    {
        BOOST_LOG_TRIVIAL(info) << "[SERVER] handle_accept with error";
        if(ec) { return; }

        BOOST_LOG_TRIVIAL(info) << "[SERVER] handle_accept with no error";
        handler->start();

        auto new_handler = std::make_shared<session>(m_io_context, m_routingTable);

        acceptor_.async_accept( new_handler->socket(),
                                [=] (auto ec)
                                {
                                    handle_new_connection( new_handler, ec);
                                }
                              );
    }



    void stabilize();

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
    /* tcp::socket m_socket; */
    int thread_count_;
    std::vector<std::thread> thread_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;
    //TODO: timestamp for uptime (part of peerinfo)
};

#endif /* SERVER_H */
