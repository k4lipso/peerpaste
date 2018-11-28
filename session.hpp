#ifndef SESSION_H
#define SESSION_H

#include "message.hpp"
/* #include "cryptowrapper.hpp" */
#include "routingTable.hpp"

#include <string>
#include <future>
#include <thread>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

using boost::asio::ip::tcp;

class session
    : public std::enable_shared_from_this<session>
{
public:
    typedef std::shared_ptr<session> SessionPtr;
    typedef std::shared_ptr<RoutingTable> RoutingPtr;
    typedef std::shared_ptr<Request> RequestPtr;
    session(tcp::socket socket)
        : socket_(std::move(socket))
    {}

    session(boost::asio::io_context& io_context, RoutingPtr routingTable)
        : socket_(io_context),
          m_routingTable(routingTable)
    {
        BOOST_LOG_TRIVIAL(info) << "[SESSION] Session Created";
        BOOST_LOG_TRIVIAL(info) << "[SESSION] m_self ID: "
                                << m_routingTable->get_self()->getID();

        m_packed_request = PackedMessage<Request>(std::make_shared<Request>());
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

    static SessionPtr create(boost::asio::io_context& io_context, RoutingPtr routingTable)
    {
        /* return std::make_shared<session>(socket, routingTable); */
        return SessionPtr(new session(io_context, routingTable));
    }

    tcp::socket& get_socket()
    {
        //TODO:: change to m_socket
        return socket_;
    }

    void start()
    {
        BOOST_LOG_TRIVIAL(info) << "[SESSION] Session started";
        do_read_header();
    }

    /**
     * Joins a Ring using a known Node inside of that ring
     * that Node will lookup and return a successor Recursively
     * returns -1 when failed
    */
    bool join(const tcp::resolver::results_type& endpoints)
    {
        BOOST_LOG_TRIVIAL(info) << "[SESSION] SESSION::join";
        boost::asio::async_connect(socket_, endpoints,
                [this, endpoints](boost::system::error_code ec, tcp::endpoint)
                {
                    if(!ec)
                    {
                        if(!query())
                        {
                            BOOST_LOG_TRIVIAL(info) << "[SESSION] Query Request failed";
                            return false;
                        }

                        /* auto self = m_routingTable->get_self(); */
                        /* auto id = self->getID(); */
                        do_read_header();

                        /* std::cout << "wait for id to bet setted" << std::endl; */
                        /* /1* while(id == self->getID()){ *1/ */
                        /* /1*     //wait *1/ */
                        /* /1* } *1/ */



                        /* std::this_thread::sleep_for(std::chrono::seconds(2)); */
                        /* std::cout << "slept 2 seconds" << std::endl; */

                        /* auto successor = m_routingTable->get_successor(); */

                        /* successor = remote_find_successor(self); */

                        /* /1* if(!find_successor(m_routingTable->get_self()->getID())) *1/ */
                        /* /1* { *1/ */
                        /* /1*     BOOST_LOG_TRIVIAL(info) << "[SESSION] Could not find Successor"; *1/ */
                        /* /1*     return false; *1/ */
                        /* /1* } *1/ */

                        /* /1* do_read_header(); *1/ */
                        /* std::this_thread::sleep_for(std::chrono::seconds(2)); */
                        /* std::cout << "slept 2 seconds" << std::endl; */

                        return true;
                    }
                    //Add timeout functionality
                    join(endpoints);
                });
        /* m_predecessor = nullptr; */
        return true;
    }

    //peer is peer object to update
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
        if(successor == nullptr){
            auto predecessor = closest_preceding_node(id);
            if(predecessor->getID() == self->getID()){
                std::cout << "find_successor() returns self" << std::endl;
                return self;
            }
            std::cout << "closest_preceding_node: " << predecessor->getID() << std::endl;
            std::cout << "self id: " << self->getID() << std::endl;
            return remote_find_successor(predecessor, id);
        }

        //if(id in range(self, successor))
        //TODO: check if comparision is okay since it is '(n, successor]' in the paper:
        //what means '(' and what means '['
        if(id.compare(self->getID()) >= 0 && id.compare(successor->getID()) <= 0)
        {
            //return successor
            return successor;
        } else {
            auto predecessor = closest_preceding_node(id);
            return remote_find_successor(predecessor, id);
        }
    }


    const std::shared_ptr<Peer> remote_find_successor(std::shared_ptr<Peer> peer, const std::string& id)
    {
        //This fct sends a find_successor request which will trigger find_successor
        //on the remote peer

        //TODO: generate find_successor request
        //find a way that we handle the response in this function so that we can return
        //the peer object in this function
        BOOST_LOG_TRIVIAL(info) << "[SESSION] SESSION::remote_find_successor";

        Request request;

        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());
        auto message = msg.get_msg();
        auto peerinfo = message->add_peerinfo();
        peerinfo->set_peer_id(id);
        peerinfo->set_peer_ip("");
        peerinfo->set_peer_rtt("");
        peerinfo->set_peer_uptime("");

        msg.init_header(true, 0, 0, "find_successor", "top_secret_id", VERSION);

        msg.pack(writebuf);

        /* tcp::resolver resolver(m_io_context); */
        /* auto endpoints = resolver.resolve(peer->getIP(), 1337); */

        //new connection
        boost::asio::write(socket_, boost::asio::buffer(writebuf));

        peer_to_wait_for = nullptr;
        do_read_header();
        /* boost::packaged_task<int> pt(&session::do_read_header); */
        /* boost:: future<int> fi=pt.get_future(); */
        /* std::packaged_task<int()> task(&session::do_read_header); */
        /* std::future<int> f1 = task.get_future(); */
        /* std::thread(std::move(task)).detach(); */

        /* f1.wait(); */
        /* std::cout << "F1: " << f1.get() << std::endl; */
        /* do_read_header(); */

        /* boost::system::error_code error; */
        /* m_readbuf.resize(HEADER_SIZE); */
        /* socket_.read_some(boost::asio::buffer(m_readbuf), error); */
        /* unsigned msg_len = m_packed_request.decode_header(m_readbuf); */
        /* std::cout << "MSG_LEN: " << msg_len << std::endl; */


        //TODO: FIND BETTER WAY WTF!
        while(peer_to_wait_for == nullptr){
            //waiting
        }
        /* peer_to_wait_for.wait(); */
        /* return std::make_shared<Peer>(peer_to_wait_for.get()); */
        return std::make_shared<Peer>(*peer_to_wait_for);
    }

private:

    const std::shared_ptr<Peer> remote_find_successor(std::shared_ptr<Peer> peer)
    {
        BOOST_LOG_TRIVIAL(info) << "[SESSION] SESSION::remote_find_successor()";

        Request request;

        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());
        auto message = msg.get_msg();
        auto peerinfo = message->add_peerinfo();
        peerinfo->set_peer_id(peer->getID());
        peerinfo->set_peer_ip(peer->getIP());
        peerinfo->set_peer_rtt("");
        peerinfo->set_peer_uptime("");

        msg.init_header(true, 0, 0, "find_successor", "top_secret_id", VERSION);

        msg.pack(writebuf);

        /* tcp::resolver resolver(m_io_context); */
        /* auto endpoints = resolver.resolve(peer->getIP(), 1337); */

        //new connection
        boost::asio::write(socket_, boost::asio::buffer(writebuf));

        /* peer_to_wait_for = nullptr; */
        do_read_header();

        std::this_thread::sleep_for(std::chrono::seconds(2));

        //TODO: FIND BETTER WAY WTF!
        /* while(peer_to_wait_for == nullptr){ */

        /*     //waiting */
        /* } */
        /* peer_to_wait_for.wait(); */
        /* return std::make_shared<Peer>(peer_to_wait_for.get()); */
        return peer;
    }
    void handle_read_header(const boost::system::error_code& error)
    {
        BOOST_LOG_TRIVIAL(debug) << "handle_read_header()";
        if(!error)
        {
            unsigned msg_len = m_packed_request.decode_header(m_readbuf);
            do_read_message(msg_len);
            return;

        } else {
            BOOST_LOG_TRIVIAL(error) << "[SESSION] error occured: " << error;
            do_read_header();
        }

        return;
    }

    int do_read_header()
    {
        BOOST_LOG_TRIVIAL(debug) << "do_read_header()";
        m_readbuf.resize(HEADER_SIZE);
        boost::asio::async_read(socket_, boost::asio::buffer(m_readbuf),
            boost::bind(&session::handle_read_header, shared_from_this(),
            boost::asio::placeholders::error));

        return 1;
    }

    bool check_precending_node(/*ID*/);

    void handle_message()
    {
        BOOST_LOG_TRIVIAL(debug) << "handle_read_message()";
        if(m_packed_request.unpack(m_readbuf))
        {
            std::cout << "unpacked" << std::endl;
            RequestPtr req = m_packed_request.get_msg();
            auto commonheader = req->commonheader();
            /* m_message = &req.get(); */

            /* //TODO: recheck if else is neccessarry */
            if(commonheader.t_flag()){
                if(!handle_request(req))
                {
                    do_read_header();
                    return;
                }
            } else {
                if(!handle_response(req))
                {
                    do_read_header();
                    return;
                }
            }
        }
        std::cout << "Could not unpack" << std::endl;
    }

    void handle_read_message(const boost::system::error_code& ec)
    {
        BOOST_LOG_TRIVIAL(debug) << "handle_read_message()";
        if(!ec){
            handle_message();
        }
    }

    void do_read_message(unsigned msg_len)
    {
        BOOST_LOG_TRIVIAL(debug) << "do_read_message()";

        m_readbuf.resize(HEADER_SIZE + msg_len);
        boost::asio::mutable_buffers_1 buf = boost::asio::buffer(&m_readbuf[HEADER_SIZE], msg_len);
        boost::asio::async_read(socket_, buf,
                boost::bind(&session::handle_read_message, shared_from_this(),
                    boost::asio::placeholders::error));
    }

    bool handle_request(RequestPtr req)
    {
        //determine request type
        auto commonheader = req->commonheader();
        auto request_type = commonheader.request_type();
        BOOST_LOG_TRIVIAL(debug) << "[Session] handle_request() request_type: "
                                 << request_type;
        //handle request type
        if(request_type == "query")
        {
            /* if(!handle_query()) return false; */
            return handle_query(req);
        }
        if(request_type == "find_successor"){
            return handle_find_successor(req);
        } else {
            BOOST_LOG_TRIVIAL(debug) << "Unknown Request Type";
            return false;
        }

        return true;
    }

    bool handle_response(RequestPtr req)
    {
        //determine request type
        //TODO: change get_request_type to more general name
        auto commonheader = req->commonheader();
        auto request_type = commonheader.request_type();
        BOOST_LOG_TRIVIAL(debug) << "[Session] handle_response() request_type: "
                                 << request_type;
        //handle request type
        if(request_type == "query")
        {
            /* BOOST_LOG_TRIVIAL(info) << "[SESSION] m_self ID: " */
            /*                         << m_routingTable->get_self()->getID(); */
            if(!handle_query_respone(req)) return false;
            /* if(!find_successor(m_routingTable->get_self()->getID())) return false; */
            /* find_successor(m_routingTable->get_self()->getID()); */
        } else {
            BOOST_LOG_TRIVIAL(debug) << "Unknown Request Type";
            return false;
        }
        return true;
    }

    std::shared_ptr<Peer> closest_preceding_node(const std::string& id)
    {
        BOOST_LOG_TRIVIAL(info) << "[Session] closest_preceding_node()";

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
        for(int i = finger->size() - 1; i >= 0; i--)
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


    bool handle_query_respone(RequestPtr req)
    {
        BOOST_LOG_TRIVIAL(info) << "[SESSION] handle_query_response";
        //TODO: extract peerinfo from Request and update own Peer information (Hash)

        //get own Peer object
        auto self = m_routingTable->get_self();
        //set new ID
        self->setID(req->peerinfo(0).peer_id());
        self->setIP(req->peerinfo(0).peer_ip());
        std::cout << "GETIP: " << self->getIP() << std::endl;
        //get predecessor/successor object
        auto predecessor = m_routingTable->get_predecessor();
        auto successor = m_routingTable->get_successor();

        //TODO: WHY/? WHY ERROR?
        /* auto predecessor = nullptr; */

        if(self->getID() != "UNKNOWN"){
            /* boost::system::error_code ec; */
            /* socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec); */
            /* //closing socket */
            /* socket_.close(); */
            return true;
        }

        return false;
    }

    bool handle_find_successor(RequestPtr req)
    {
        BOOST_LOG_TRIVIAL(info) << "[SESSION] SESSION::handle_find_successor";
        //check if peerinfo has exactly one member
        if(req->peerinfo_size() != 1){
            return false;
        }

        auto peer = find_successor(req->peerinfo(0).peer_id());

        Request request;

        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());
        auto message = msg.get_msg();
        auto peerinfo = message->add_peerinfo();

        peerinfo->set_peer_id(peer->getID());
        peerinfo->set_peer_ip(peer->getIP());
        peerinfo->set_peer_rtt(peer->getIP());
        peerinfo->set_peer_uptime(peer->getIP());

        msg.init_header(false, 0, 0, "find_successor", "top_secret_id", VERSION);
        msg.pack(writebuf);
        boost::asio::write(socket_, boost::asio::buffer(writebuf));
        std::cout << "Wrote find_successor response" << std::endl;

    }

    bool handle_query(RequestPtr req)
    {
        BOOST_LOG_TRIVIAL(info) << "[SESSION] SESSION::handle_query";
        //Query Request contains PeerInfo peer_id = "UNKNOWN";
        //this has to be updated to a hash of the ip addr of that peer
        //also P2P-options containing hash-algo,
        //DHT-algo ect should be contained in the response

        //check if peerinfo has exactly one member
        if(req->peerinfo_size() != 1){
            return false;
        }

        //TODO: create hashing helper class that handles hashing of files and strings
        auto client_ip = socket_.remote_endpoint().address().to_string();
        auto client_hash = util::generate_sha256(client_ip);

        BOOST_LOG_TRIVIAL(info) << "[SESSION] Query Received";
        BOOST_LOG_TRIVIAL(info) << "[SESSION] Client IP: " << client_ip;
        BOOST_LOG_TRIVIAL(info) << "[SESSION] New Client HASH: " << client_hash;

        Request request;

        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());
        auto message = msg.get_msg();
        auto peerinfo = message->add_peerinfo();

        peerinfo->set_peer_id(client_hash);
        peerinfo->set_peer_ip(client_ip);
        peerinfo->set_peer_rtt(client_ip);
        peerinfo->set_peer_uptime(client_ip);

        msg.init_header(false, 0, 0, "query", "top_secret_id", VERSION);
        msg.pack(writebuf);
        boost::asio::write(socket_, boost::asio::buffer(writebuf));

        //create response message
        //TODO: extra fctn
        //TODO: Change "Request" to something like "Message" in .proto
        /* Request request; */
        /* auto peerinfo = request.add_peerinfo(); */
        /* peerinfo->set_peer_ip(client_ip); */
        /* peerinfo->set_peer_id(client_hash); */
        /* peerinfo->set_peer_uptime(client_hash); */
        /* peerinfo->set_peer_rtt(client_hash); */

        /* std::cout << "peerinfo hash: " << peerinfo->peer_id() << std::endl; */

        /* Message message; //needed to generate common header?! -.- */
        /* message.set_common_header(request.mutable_commonheader(), */
        /*                              false, */
        /*                              0, */
        /*                              0, */
        /*                              "query", */
        /*                              "top_secret_transaction_id", */
        /*                              VERSION); */

        /* message.serialize_object(request); */
        /* boost::asio::write(socket_, *message.output()); */

        BOOST_LOG_TRIVIAL(info) << "[SESSION] Sent query_response";
        /* do_read_header(); */

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
                            do_read_header();
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
                            do_read_header();
                            return;
                        }
                        if(!m_message.decode_peerinfo(size))
                        {
                            do_read_header();
                            return;
                        }
                        //TODO: Change method to accept further message objects
                        //other than common header
                        do_read_header();
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
        //TODO:: CHANGE TO USE MSG.PACK()!!!!!
        BOOST_LOG_TRIVIAL(info) << "[SESSION] SESSION::query";
        //Creating PeerInfo with empty ID
        //This must be done before creating the CommonHeader because message_length
        //must be known when it gets encoded
        //TODO: still needed?
        uint32_t message_length(0);
        //Create a Request
        Request request;

        //TODO: rename
        std::shared_ptr<PeerInfo> peerinfo_ = m_routingTable->get_self()->getPeer();

        //Set the message
        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());
        auto message = msg.get_msg();
        auto peerinfo = message->add_peerinfo();
        peerinfo->set_peer_id(peerinfo_->peer_id());
        peerinfo->set_peer_ip(peerinfo_->peer_ip());
        peerinfo->set_peer_rtt(peerinfo_->peer_ip());
        peerinfo->set_peer_uptime(peerinfo_->peer_ip());

        /* auto commonheader = message->mutable_commonheader(); */
        /* commonheader->set_t_flag = 1; */
        /* commonheader->set_ttl = 0; */
        /* commonheader->set_message_length = 0; */
        /* commonheader->set_request_type = "query"; */
        /* commonheader->set_t_flag = 1; */
        /* commonheader->set_t_flag = 1; */


        /* auto header = message->mutable_commonheader(); */
        msg.init_header(true, 0, 0, "query", "top_secret_id", VERSION);

        msg.pack(writebuf);

        boost::asio::write(socket_, boost::asio::buffer(writebuf));

        return true;
    }

    std::string Put(int level_of_decryption);

    std::string Get(size_t hash);

    void do_write(std::size_t length);

    RoutingPtr m_routingTable;

    Message m_message;
    tcp::socket socket_;

    Peer *peer_to_wait_for;

    std::vector<uint8_t> m_readbuf;
    PackedMessage<Request> m_packed_request;
};

#endif /* SESSION_H */
