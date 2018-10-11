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
        BOOST_LOG_TRIVIAL(info) << "Server Initialized";
        create();
        BOOST_LOG_TRIVIAL(info) << "Start listening on Port " << port;
        do_accept();
    }

    server(boost::asio::io_context& io_context, short port,
           const tcp::resolver::results_type& endpoints)
        : m_io_context(io_context),
          m_socket(io_context),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
        join(endpoints);
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
        const size_t id = std::hash<std::string>{}(ip);
        BOOST_LOG_TRIVIAL(info) << "IP: " << ip;
        BOOST_LOG_TRIVIAL(info) << "ID: " << id;
        return Peer(id, ip);
    }

    //global IP is needed for creating own PeerObject
    //NAT :(
    void create()
    {
        m_predecessor = nullptr;
        auto p = std::make_shared<Peer>(createPeer());
        m_self = p;
        m_successor = p;
    }

    /**
     * Joins a Ring using a known Node inside of that ring
     * that Node will lookup and return a successor Recursively
     * returns -1 when failed
    */
    bool join(const tcp::resolver::results_type& endpoints)
    {
        boost::asio::async_connect(m_socket, endpoints,
                [this](boost::system::error_code ec, tcp::endpoint)
                {
                    if(!ec)
                    {
                        if(!query())
                        {
                            BOOST_LOG_TRIVIAL(error) << "Query Request failed";
                            return false;
                        }

                    }
                });
        m_predecessor = nullptr;
        return true;
    }

    /**
     * A node MUST send a Query request to a discovered node before a join request.
     * Query request is used to determine overlay parameters such as overlay-ID,
     * peer-to-peer and hash algo, request routing method (recur vs iter).
     */
    bool query()
    {
        //Creating PeerInfo with empty ID
        //This must be done before creating the CommonHeader because message_length
        //must be known when it gets encoded
        uint32_t message_length(0);
        //Create a Request
        Request request;

        auto peerinfo = request.add_peerinfo();
        peerinfo->set_peer_id("UNKNOWN");
        message_length += peerinfo->ByteSize() + 1;

        //Create Message and add CommonHeader with request type "query"
        Message message;

        message.set_common_header(request.mutable_commonheader(),
                                     true,
                                     0,
                                     message_length,
                                     "query",
                                     "top_secret_transaction_id",
                                     VERSION);

        message.serialize_object(request);

        //Send the message
        boost::asio::write(m_socket, *message.output());

        return true;
    }

    void stabilize();

    void notify(/*IP*/);

    void fix_fingers();

    void check_predecessor();

    void do_accept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket)
            {
                if (!ec)
                {
                    std::make_shared<session>(std::move(socket))->start();
                }

                do_accept();
            });
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
    std::shared_ptr<RoutingTable> m_fingerTable;

    boost::asio::io_context& m_io_context;
    tcp::socket m_socket;

    //TODO: timestamp for uptime (part of peerinfo)

};

#endif /* SERVER_H */
