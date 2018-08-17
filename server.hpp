#ifndef SERVER_H
#define SERVER_H

#include "routingTable.hpp"
#include "session.hpp"
#include "peer.hpp"

#include <string>
#include <functional>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>

using boost::asio::ip::tcp;

class server
{
public:
    server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        BOOST_LOG_TRIVIAL(info) << "Server Initialized";
        create();
        BOOST_LOG_TRIVIAL(info) << "Start listening on Port " << port;
        do_accept();
    }

    server(boost::asio::io_context& io_context, short port, std::string ip)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        join(ip);
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
    int join(std::string ip)
    {
        m_predecessor = nullptr;
        return 0;
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
};

#endif /* SERVER_H */
