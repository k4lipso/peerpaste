#ifndef SESSION_H
#define SESSION_H

#include "message.hpp"
#include "cryptowrapper.hpp"
#include "routingTable.hpp"

#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class session
    : public std::enable_shared_from_this<session>
{
public:
    session(tcp::socket socket)
        : socket_(std::move(socket))
    {}

    session(tcp::socket socket, std::shared_ptr<RoutingTable> routingTable)
        : socket_(std::move(socket)),
          m_routingTable(std::move(routingTable))
    {
        BOOST_LOG_TRIVIAL(info) << "Session Created";
        BOOST_LOG_TRIVIAL(info) << "m_self ID: "
                                << m_routingTable->get_self()->getID();
    }

    /**
     * using this constructor an "outgoing" session will be created.
     * if request_type is "join" for example, that session will start
     * a request a query at the given endpoint.
     * so instead the server handles join/query whatever, everything
     * is handled by session object. so the server object just has to
     * initialize a session object which then handles all the messages
     */
    /* session(tcp::socket socket, const tcp::resolver::results_type& endpoints, */
    /*         std::string request_type) */
    /*     : socket_(std::move(socket)) */
    /* {} */

    void start()
    {
        BOOST_LOG_TRIVIAL(info) << "Session started";
        do_read_varint();
    }

    /**
     * Joins a Ring using a known Node inside of that ring
     * that Node will lookup and return a successor Recursively
     * returns -1 when failed
    */
    bool join(const tcp::resolver::results_type& endpoints)
    {
        BOOST_LOG_TRIVIAL(info) << "SESSION::join";
        boost::asio::async_connect(socket_, endpoints,
                [this, endpoints](boost::system::error_code ec, tcp::endpoint)
                {
                    if(!ec)
                    {
                        if(!query())
                        {
                            BOOST_LOG_TRIVIAL(info) << "Query Request failed";
                            return false;
                        }

                        /* if(!find_successor()) */
                        /* { */
                        /*     BOOST_LOG_TRIVIAL(info) << "Could find Successor"; */
                        /*     return false; */
                        /* } */
                        return true;
                    }
                    //Add timeout functionality
                    join(endpoints);
                });
        /* m_predecessor = nullptr; */
        return true;
    }

private:
    void do_read_varint()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                //Read 1 Byte which should be an Varint
                *m_message.data(), boost::asio::transfer_exactly(1),
                [this, self](boost::system::error_code ec, std::size_t)
                {
                    if(!ec){
                        uint32_t size;
                        if(!m_message.get_varint(size))
                        {
                            do_read_varint();
                            return;
                        }

                        //everything worked, read body using the size
                        //extraced from the varint
                        do_read_message(size);
                        return;
                    }

                    do_read_varint();
                });
    }

    bool check_precending_node(/*ID*/);

    void do_read_message(const uint32_t& size)
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                *m_message.data(), boost::asio::transfer_exactly(size),
                [this, self, size](boost::system::error_code ec, std::size_t)
                {
                    if(!ec){
                        if(!m_message.decode_message(size))
                        {
                            do_read_varint();
                            return;
                        }
                        //TODO: Change method to accept further message objects
                        //other than common header,
                        //a message evaluation chain has do be developed to receive
                        //a sequence of different objects depending on the
                        //common_header request_tpe
                        //
                        //idea is to create request/response objects using protobuf so
                        //that only one object has to be transmitted each time
                        /* do_read_objects(m_message.get_message_length()); */

                        std::string type("RESPONSE");
                        if(m_message.get_t_flag()) type = "REQUEST";
                        BOOST_LOG_TRIVIAL(info) << type << " Received";

                        //TODO: recheck if else is neccessarry
                        if(m_message.get_t_flag()){
                            if(!handle_request())
                            {
                                do_read_varint();
                                return;
                            }
                        } else {
                            if(!handle_response())
                            {
                                /* do_read_varint(); */
                                return;
                            }
                        }


                    }

                });
    }

    bool handle_request()
    {
        //determine request type
        auto request_type = m_message.get_request_type();
        BOOST_LOG_TRIVIAL(debug) << "[Session] handle_request() request_type: "
                                 << request_type;
        //handle request type
        if(request_type == "query")
        {
            if(!handle_query()) return false;
        } else {
            BOOST_LOG_TRIVIAL(debug) << "Unknown Request Type";
            return false;
        }

        return true;
    }

    bool handle_response()
    {
        //determine request type
        //TODO: change get_request_type to more general name
        auto request_type = m_message.get_request_type();
        BOOST_LOG_TRIVIAL(debug) << "[Session] handle_response() request_type: "
                                 << request_type;
        //handle request type
        if(request_type == "query")
        {
            /* BOOST_LOG_TRIVIAL(info) << "m_self ID: " */
            /*                         << m_routingTable->get_self()->getID(); */
            if(!handle_query_respone()) return false;
            /* if(!find_successor(m_routingTable->get_self()->getID())) return false; */
        } else {
            BOOST_LOG_TRIVIAL(debug) << "Unknown Request Type";
            return false;
        }
        return true;
    }

    const std::shared_ptr<Peer> find_successor(const std::string& id)
    {
        /*
        ask node n to find the successor of id
        n.find successor(id)
            if (id ∈ (n, successor])
                return successor;
            else
                n0 = closest preceding node(id );
                return n0.find successor(id);
        */

        //TODO: check if id is in reange (n, successor) by comparing strings using str.compare(str)
        auto self = m_routingTable->get_self();
        auto successor = m_routingTable->get_successor();

        //if(id in range(self, successor))
        //TODO: check if comparision is okay since it is '(n, successor]' in the paper:
        //what means '(' and what means '['
        if(id.compare(self->getID()) >= 0 && id.compare(successor->getID()) <= 0)
        {
            //return successor
            return successor;
        } else {
            auto predecessor = closest_preceding_node(id);
            return remote_find_successor(predecessor);
        }
    }

    const std::shared_ptr<Peer> remote_find_successor(std::shared_ptr<Peer> peer)
    {
        //This fct sends a find_successor request which will trigger find_successor
        //on the remote peer

        //TODO: generate find_successor request
        //find a way that we handle the response in this function so that we can return
        //the peer object in this function
        BOOST_LOG_TRIVIAL(info) << "SESSION::remote_find_successor";

        uint32_t message_length(0);

        Request request;

    }

    std::shared_ptr<Peer> closest_preceding_node(const std::string& id)
    {
        /*
        // search the local table for the highest predecessor of id
        n.closest preceding node(id)
        for i = m downto 1
            if (finger[i] ∈ (n, id))
                return finger[i];
        return n;
        */
        auto self = m_routingTable->get_self();
        auto finger = m_routingTable->get_fingerTable();
        for(int i = finger->size(); i <= 0; i--)
        /* for(auto finger = m_routingTable->m_fingerTable.end(); */
        /*          finger >= m_routingTable->m_fingerTable.begin(); */
        /*          finger--) */
        {
            std::string finger_id = finger->at(i)->getID();
            if(finger_id.compare(self->getID()) >= 0 && finger_id.compare(id) <= 0){
                return finger->at(i);
            }
        }
        return self;
    }


    bool handle_query_respone()
    {
        //TODO: extract peerinfo from Request and update own Peer information (Hash)

        //get own Peer object
        auto self = m_routingTable->get_self();
        //set new ID
        self->setID(m_message.get_peer_id());

        //get predecessor/successor object
        auto predecessor = m_routingTable->get_predecessor();
        auto successor = m_routingTable->get_successor();

        //TODO: WHY/? WHY ERROR?
        /* auto predecessor = nullptr; */

        if(self->getID() != "UNKNOWN"){
            return true;
        }

        return false;
    }

    bool handle_query()
    {
        BOOST_LOG_TRIVIAL(info) << "SESSION::handle_query";
        //Query Request contains PeerInfo peer_id = "UNKNOWN";
        //this has to be updated to a hash of the ip addr of that peer
        //also P2P-options containing hash-algo,
        //DHT-algo ect should be contained in the response

        //check if peerinfo has exactly one member
        if(m_message.get_Message()->peerinfo_size() != 1){
            return false;
        }

        //TODO: create hashing helper class that handles hashing of files and strings
        auto client_ip = socket_.remote_endpoint().address().to_string();
        auto client_hash = util::generate_sha256(client_ip);

        BOOST_LOG_TRIVIAL(info) << "Query Received";
        BOOST_LOG_TRIVIAL(info) << "Client IP: " << client_ip;
        BOOST_LOG_TRIVIAL(info) << "New Client HASH: " << client_hash;

        //create response message
        //TODO: extra fctn
        //TODO: Change "Request" to something like "Message" in .proto
        Request request;
        auto peerinfo = request.add_peerinfo();
        peerinfo->set_peer_ip(client_ip);
        peerinfo->set_peer_id(client_hash);
        peerinfo->set_peer_uptime(client_hash);
        peerinfo->set_peer_rtt(client_hash);

        std::cout << "peerinfo hash: " << peerinfo->peer_id() << std::endl;

        Message message; //needed to generate common header?! -.-
        message.set_common_header(request.mutable_commonheader(),
                                     false,
                                     0,
                                     0,
                                     "query",
                                     "top_secret_transaction_id",
                                     VERSION);

        message.serialize_object(request);
        boost::asio::write(socket_, *message.output());

        BOOST_LOG_TRIVIAL(info) << "Sent query_response";

        return true;
    }

    bool send_response()
    {
        return false;
    }

    [[deprecated]]
    void do_read_common_header(const uint32_t& size)
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                *m_message.data(), boost::asio::transfer_exactly(size),
                [this, self, size](boost::system::error_code ec, std::size_t)
                {
                    if(!ec){
                        if(!m_message.decode_header(size))
                        {
                            do_read_varint();
                            return;
                        }
                        //TODO: Change method to accept further message objects
                        //other than common header,
                        //a message evaluation chain has do be developed to receive
                        //a sequence of different objects depending on the
                        //common_header request_tpe
                        //
                        //idea is to create request/response objects using protobuf so
                        //that only one object has to be transmitted each time
                        do_read_objects(m_message.get_message_length());
                    }

                });
    }

    [[deprecated]]
    void do_read_objects(const uint32_t& size)
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
                *m_message.data(), boost::asio::transfer_exactly(size),
                [this, self, size](boost::system::error_code ec, std::size_t)
                {
                    if(!ec){
                        if(!m_message.decode_peerinfo(size))
                        {
                            do_read_varint();
                            return;
                        }
                        if(!m_message.decode_peerinfo(size))
                        {
                            do_read_varint();
                            return;
                        }
                        //TODO: Change method to accept further message objects
                        //other than common header
                        do_read_varint();
                    }

                });
    }

    /**
     * A node MUST send a Query request to a discovered node before a join request.
     * Query request is used to determine overlay parameters such as overlay-ID,
     * peer-to-peer and hash algo, request routing method (recur vs iter).
     */
    bool query()
    {
        BOOST_LOG_TRIVIAL(info) << "SESSION::query";
        //Creating PeerInfo with empty ID
        //This must be done before creating the CommonHeader because message_length
        //must be known when it gets encoded
        //TODO: still needed?
        uint32_t message_length(0);
        //Create a Request
        Request request;
        /* auto repeated_field_ptr = request.mutable_peerinfo(); */

        std::shared_ptr<PeerInfo> peerinfo_ = m_routingTable->get_self()->getPeer();
        auto peerinfo = request.add_peerinfo();
        /* *peerinfo = *peerinfo_; */
        /* PeerInfo peerinfo2 = *peerinfo; */
        /* PeerInfo peerinfo; */

        /* repeated_field_ptr->Add(peerinfo); */
        /* request.add_peerinfo(peerinfo); */
        peerinfo->set_peer_id(peerinfo_->peer_id());
        peerinfo->set_peer_ip(peerinfo_->peer_ip());
        peerinfo->set_peer_rtt(peerinfo_->peer_ip());
        peerinfo->set_peer_uptime(peerinfo_->peer_ip());
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
        boost::asio::write(socket_, *message.output());

        return true;
    }

    std::string Put(int level_of_decryption);

    std::string Get(size_t hash);

    void do_write(std::size_t length);

    std::shared_ptr<RoutingTable> m_routingTable;

    Message m_message;
    tcp::socket socket_;
};

#endif /* SESSION_H */
