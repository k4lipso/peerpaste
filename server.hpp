#ifndef SERVER_H
#define SERVER_H

#define VERSION "0.0.1"

#include "routingTable.hpp"
#include "session.hpp"
#include "peer.hpp"
#include "message.hpp"
#include "proto/messages.pb.h"

#include <string>
#include <sstream>
#include <functional>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/log/trivial.hpp>

using boost::asio::ip::tcp;

class server
{
public:
    server(boost::asio::io_context& io_context, short port)
        : m_io_context(io_context),
          m_socket(io_context),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        //TODO: initialize m_self (Peer)
        BOOST_LOG_TRIVIAL(info) << "[SERVER] Server Initialized";
        create();
        BOOST_LOG_TRIVIAL(info) << "[SERVER] Start listening on Port " << port;
        do_accept();
    }

    server(boost::asio::io_context& io_context, short port,
           const tcp::resolver::results_type& endpoints)
        : m_io_context(io_context),
          m_socket(io_context),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        create();
        join(endpoints);
        BOOST_LOG_TRIVIAL(info) << "[SERVER] OWN ID: " << m_routingTable->get_self()->getID();
        BOOST_LOG_TRIVIAL(info) << "[SERVER] Server Initialized";
        BOOST_LOG_TRIVIAL(info) << "[SERVER] Start listening on Port " << port;
        do_accept();
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
        const std::string id = "UNKNOWN";
        BOOST_LOG_TRIVIAL(info) << "[SERVER] IP: " << ip;
        BOOST_LOG_TRIVIAL(info) << "[SERVER] ID: " << id;
        return Peer(id, ip);
    }

    //global IP is needed for creating own PeerObject
    //NAT :(
    void create()
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
    bool join(const tcp::resolver::results_type& endpoints)
    {
        auto sesh = session::create(m_io_context, m_routingTable);
        /* sesh->start(); */
        sesh->join(endpoints);

        //TODO: somehow block here
        /* boost::asio::steady_timer timer{m_io_context, std::chrono::seconds{3}}; */
        /* timer.async_wait([](const boost::system::error_code &ec) */
        /*     { std::cout << "3 sec\n"; }); */

        /* while(m_routingTable->get_self()->getID() == "UNKNOWN") { */
        /*     std::cout << "wait for foo" << std::endl; */
        /* } */

        return false;
    }

    /**
     * A node MUST send a Query request to a discovered node before a join request.
     * Query request is used to determine overlay parameters such as overlay-ID,
     * peer-to-peer and hash algo, request routing method (recur vs iter).
     */
    bool query()
    {
        /* //Creating PeerInfo with empty ID */
        /* //This must be done before creating the CommonHeader because message_length */
        /* //must be known when it gets encoded */
        /* uint32_t message_length(0); */
        /* //Create a Request */
        /* Request request; */

        /* auto peerinfo = request.add_peerinfo(); */
        /* peerinfo->set_peer_id("UNKNOWN"); */
        /* message_length += peerinfo->ByteSize() + 1; */

        /* //Create Message and add CommonHeader with request type "query" */
        /* Message message; */

        /* message.set_common_header(request.mutable_commonheader(), */
        /*                              true, */
        /*                              0, */
        /*                              message_length, */
        /*                              "query", */
        /*                              "top_secret_transaction_id", */
        /*                              VERSION); */

        /* message.serialize_object(request); */

        /* //Send the message */
        /* boost::asio::write(m_socket, *message.output()); */

        return false;
    }

    void stabilize();

    void notify(/*IP*/);

    void fix_fingers();

    void check_predecessor();

    void do_accept()
    {
        BOOST_LOG_TRIVIAL(info) << "[SERVER] do_accept";
        session::SessionPtr new_connection =
            session::create(m_io_context, m_routingTable);

        acceptor_.async_accept(new_connection->get_socket(),
                boost::bind(&server::handle_accept, this, new_connection,
                    boost::asio::placeholders::error));
    }

    void handle_accept(session::SessionPtr session,
            const boost::system::error_code& ec)
    {
        if(!ec) {
            BOOST_LOG_TRIVIAL(info) << "[SERVER] handle_accept with no error";
            session->start();
            std::cout << "MY ID U FOOL: " << m_routingTable->get_self()->getID() << std::endl;

            do_accept();
        }
        BOOST_LOG_TRIVIAL(info) << "[SERVER] handle_accept with error: " << ec;
        do_accept();
    }

    tcp::acceptor acceptor_;

    //Peer Object holding information about itself
    std::shared_ptr<Peer> m_self;

    //Peer Object holding information about
    //the next peer in the Ring
    std::shared_ptr<Peer> m_successor;

    //Peer Object holding information about
    //the previous Peer in the Ring
    std::unique_ptr<const Peer> m_predecessor;

    //Fingertable containing multiple peers,
    //used to lookup keys
    //TODO: successer,predecessor,self and fingertable all in one RoutingTable object
    //MAKE READs/WRITEs ATOMIC
    //pass shared_ptr of routing table to each session
    //ATOMIC READS AND WRITES
    std::shared_ptr<RoutingTable> m_routingTable;

    boost::asio::io_context& m_io_context;
    tcp::socket m_socket;

    //TODO: timestamp for uptime (part of peerinfo)

};

#endif /* SERVER_H */
