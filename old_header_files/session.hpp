#ifndef SESSION_H
#define SESSION_H

#include "message.hpp"
#include "routingTable.hpp"

#include <string>
#include <future>
#include <thread>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

using boost::asio::ip::tcp;

//TODO: will overflow!!!!!!!!!!!!
static int naming = 0;

class Session
    : public std::enable_shared_from_this<Session>
{
public:
    typedef std::shared_ptr<Session> SessionPtr;
    typedef std::shared_ptr<RoutingTable> RoutingPtr;
    typedef std::shared_ptr<Request> RequestPtr;

    Session(boost::asio::io_context& io_context)
        : service_(io_context),
          /* io_context_(io_context), */
          socket_(io_context),
          write_strand_(io_context),
          read_strand_(io_context),
          name(std::to_string(++naming)),
          future(promise.get_future()),
          future_query(promise_query.get_future())
    {
        routing_table_ = RoutingTable::getInstance();
        readbuf_ = { 0 };
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "Session Created";
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "m_self ID: "
                                << routing_table_->get_self()->get_id();

        packed_request_ = PackedMessage<Request>(std::make_shared<Request>());
    }

    ~Session()
    {
        routing_table_->print();
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "Session Destroyed";
    }

    boost::asio::ip::tcp::socket& socket()
    {
        return socket_;
    }

    /**
     * using this constructor an "outgoing" session will be created.
     * if request_type is "join" for example, that session will start
     * tcp::resolver resolver(io_context_)
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
        return SessionPtr(new Session(io_context));
    }

    void start()
    {
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "Session started";
        do_read_header();
    }

    void send(const std::vector<uint8_t>& writebuf)
    {
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::send()";
        service_.post( write_strand_.wrap( [me = shared_from_this(), writebuf] ()
                                            {
                                                me->queue_message(writebuf);
                                            } ) );
    }

    void queue_message(const std::vector<uint8_t>& message)
    {
        bool write_in_progress = !send_packet_queue.empty();
        send_packet_queue.push_back(std::move(message));

        if(!write_in_progress)
        {
            start_packet_send();
        }
    }

    void start_packet_send()
    {
        async_write(socket_,
                    boost::asio::buffer(send_packet_queue.front()),
                    write_strand_.wrap( [me = shared_from_this()]
                                        ( boost::system::error_code const & ec,
                                          std::size_t )
                                        {
                                            me->packet_send_done(ec);
                                        }
                    ));
    }

    void packet_send_done(boost::system::error_code const & error)
    {
        if(!error)
        {
            auto vec = send_packet_queue.front();
            send_packet_queue.pop_front();

            if(!send_packet_queue.empty()){ start_packet_send(); }
        }
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "packet sent";
    }

    /**
     * Joins a Ring using a known Node inside of that ring
     * that Node will lookup and return a successor Recursively
     * returns -1 when failed
    */
    bool join(const tcp::resolver::results_type& endpoints)
    {
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::join";
        boost::asio::async_connect(socket_, endpoints,
                [this, me = shared_from_this(), endpoints]
                (boost::system::error_code ec, tcp::endpoint)
                {
                    if(!ec)
                    {
                        service_.post( read_strand_.wrap( [=] ()
                                            {
                                                me->query();
                                            } ) );

                        future_query.get();

                        service_.post( read_strand_.wrap( [=] ()
                                            {
                                                routing_table_->set_successor(
                                                        me->remote_find_successor(
                                                            routing_table_->get_self()));
                                            } ) );

                        //TODO: does this actually return something???????
                        return true;
                    }
                        BOOST_LOG_TRIVIAL(error) << get_name_tag()
                                                 << "join failed: " << ec;
                        return false;
                });
        return true;
    }

    bool stabilize()
    {
        if(routing_table_->get_successor() == nullptr){
            BOOST_LOG_TRIVIAL(info) << get_name_tag() << "stbilize: successor not initialzed. returning";
            return false;
        }
        tcp::resolver resolver(io_context_);
        //TODO: that port must be included in peerinfo!!!!!!!!!!!!!!!
        auto endpoint = resolver.resolve(routing_table_->get_successor()->get_ip(),
                                         routing_table_->get_successor()->get_port());


        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::stabilize()";
        boost::asio::async_connect(socket_, endpoint,
                [this, me = shared_from_this(), endpoint]
                (boost::system::error_code ec, tcp::endpoint)
                {
                    if(!ec)
                    {
                        service_.post( read_strand_.wrap([=] ()
                                {
                                    me->stabilize_internal();
                                } ));
                        return true;
                    }
                    BOOST_LOG_TRIVIAL(error) << get_name_tag()
                                             << "stabilize failed: " << ec;
                    return false;
                });
        return true;
    }

    bool notify()
    {
        tcp::resolver resolver(io_context_);
        //TODO: that port must be included in peerinfo!!!!!!!!!!!!!!!
        if(routing_table_->get_successor() == nullptr){
            BOOST_LOG_TRIVIAL(info) << get_name_tag() << "notify: successor not initialzed. returning";
            return false;
        }
        auto endpoint = resolver.resolve(routing_table_->get_successor()->get_ip(),
                                         routing_table_->get_successor()->get_port());


        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::notify()";
        boost::asio::async_connect(socket_, endpoint,
                [this, me = shared_from_this(), endpoint]
                (boost::system::error_code ec, tcp::endpoint)
                {
                    if(!ec)
                    {
                        service_.post( read_strand_.wrap([=] ()
                                {
                                    me->notify_internal();
                                } ));
                        return true;
                    }
                    BOOST_LOG_TRIVIAL(error) << get_name_tag()
                                             << "notify failed: " << ec;
                    return false;
                });
        return true;
    }

    bool check_predecessor()
    {
        tcp::resolver resolver(io_context_);
        //TODO: that port must be included in peerinfo!!!!!!!!!!!!!!!
        if(routing_table_->get_predecessor() == nullptr){
            BOOST_LOG_TRIVIAL(info) << get_name_tag() << "check_predecessor:" <<
                                    " predecessor not initialzed. returning";
            return false;
        }
        auto endpoint = resolver.resolve(routing_table_->get_predecessor()->get_ip(),
                                         routing_table_->get_predecessor()->get_port());


        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::check_predecessor()";
        boost::asio::async_connect(socket_, endpoint,
                [this, me = shared_from_this(), endpoint]
                (boost::system::error_code ec, tcp::endpoint)
                {
                    if(!ec)
                    {
                        //TODO: for now we just try to connect, if it works we dont change anything
                        //but we should also check if the predecessor is answering to a message
                        //sent in check_predecessor_internal()
                        /* service_.post( read_strand_.wrap([=] () */
                        /*         { */
                        /*             me->check_predecessor_internal(); */
                        /*         } )); */
                        return true;
                    }
                    BOOST_LOG_TRIVIAL(error) << get_name_tag()
                                             << "connect to predecessor failed: " << ec;
                    routing_table_->set_predecessor(nullptr);
                    return false;
                });
        return true;
    }

    std::shared_ptr<Peer> connect(const tcp::resolver::results_type& endpoints, std::string id)
    {
        boost::promise<int> promise_;
        boost::unique_future<int> future(promise_.get_future());
        std::shared_ptr<Peer> peer;
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::connect";
        boost::asio::async_connect(socket_, endpoints,
                [this, me = shared_from_this(), endpoints, &promise_, &peer, id](boost::system::error_code ec, tcp::endpoint)
                {
                    if(!ec)
                    {
                        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::connect connected";
                        peer = remote_find_successor(id);
                        promise_.set_value(1);
                    } else {
                        BOOST_LOG_TRIVIAL(error) << get_name_tag() << "error: " << ec;
                    }
                    //Add timeout functionality
                    /* me->connect(endpoints); */
                });
        /* io_context_.run(); */
        future.get();
        return peer;
    }

    //peer is peer object to update
    const std::shared_ptr<Peer> find_successor(const std::string& id)
    {
        auto self = routing_table_->get_self();
        auto successor = routing_table_->get_successor();
        if(successor == nullptr){
            auto predecessor = closest_preceding_node(id);
            if(predecessor->get_id() == self->get_id()){
                std::cout << "find_successor() returns self" << std::endl;
                return self;
            }
            std::cout << "closest_preceding_node: " << predecessor->get_id() << std::endl;
            std::cout << "self id: " << self->get_id() << std::endl;
            return remote_find_factory(predecessor, id);
        }

        auto self_id = self->get_id();
        auto succ_id = successor->get_id();
        if(util::between(self_id, id, succ_id) || id == succ_id)
        {
            std::cout << "RETURNED SUCC in find_successor" << std::endl;
            //return successor
            return successor;
        } else {
            auto predecessor = closest_preceding_node(id);
            if(predecessor->get_id() == self->get_id()){
                std::cout << "PREDECESSOR IS SELF" << std::endl;
                return self;
            }
            return remote_find_factory(predecessor, id);
        }
    }

    std::shared_ptr<Peer> remote_find_factory(std::shared_ptr<Peer> peer, const std::string& id)
    {
        tcp::resolver resolver(service_);
        //TODO: NO STATIC PORT U FOOL!
        auto endpoints = resolver.resolve(peer->get_ip(), peer->get_port());
        auto handler =
            std::make_shared<Session>(service_);
        return handler->connect(endpoints, id);
        /* return handler->remote_find_successor(id); */
    }


    std::shared_ptr<Peer> remote_find_successor(const std::string& id)
    {
        //This fct sends a find_successor request which will trigger find_successor
        //on the remote peer
        //TODO: generate find_successor request
        //find a way that we handle the response in this function so that we can return
        //the peer object in this function
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::remote_find_successor with id";
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << id;

        Request request;

        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());
        auto message = msg.get_msg();
        auto peerinfo = message->add_peerinfo();
        peerinfo->set_peer_id(id);
        peerinfo->set_peer_ip("");
        peerinfo->set_peer_port("");
        peerinfo->set_peer_rtt("");
        peerinfo->set_peer_uptime("");

        msg.init_header(true, 0, 0, "find_successor", "top_secret_id", VERSION);

        msg.pack(writebuf);

        //TODO: send in new session to new host
        send(writebuf);

        peer_to_wait_for = nullptr;
        do_read_header();

        BOOST_LOG_TRIVIAL(debug) << get_name_tag() << "Socket.is_open(): " << socket_.is_open();
        future.get();

        return peer_to_wait_for;
    }

    const std::shared_ptr<Peer> remote_find_successor(std::shared_ptr<Peer> peer)
    {
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::remote_find_successor()";

        Request request;

        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());
        auto message = msg.get_msg();
        auto peerinfo = message->add_peerinfo();
        peerinfo->set_peer_id(peer->get_id());
        peerinfo->set_peer_ip(peer->get_ip());
        peerinfo->set_peer_port(peer->get_port());
        peerinfo->set_peer_rtt("");
        peerinfo->set_peer_uptime("");

        msg.init_header(true, 0, 0, "find_successor", "top_secret_id", VERSION);

        msg.pack(writebuf);

        //TODO: send in new session to new host
        send(writebuf);

        do_read_header();

        //TODO: add timeout functionality
        BOOST_LOG_TRIVIAL(debug) << get_name_tag() << "Socket.is_open(): " << socket_.is_open();
        future.get();

        return peer_to_wait_for;
    }

private:

    void handle_read_header(const boost::system::error_code& error)
    {
        BOOST_LOG_TRIVIAL(debug) << get_name_tag() << "]handle_read_header()";
        if(!error)
        {
            unsigned msg_len = packed_request_.decode_header(readbuf_);
            do_read_message(msg_len);
            return;

        } else {
            BOOST_LOG_TRIVIAL(error) << get_name_tag() << "error occured: " << error;
            /* do_read_header(); */
        }

        return;
    }

    //TODO: this function has to post to the read_strand_
    int do_read_header()
    {
        BOOST_LOG_TRIVIAL(debug) << get_name_tag() << "]do_read_header()";
        readbuf_.resize(HEADER_SIZE);

        boost::asio::async_read(socket_,
                                boost::asio::buffer(readbuf_),
                                [me = shared_from_this()]
                                (boost::system::error_code const &error, std::size_t)
                                {
                                    me->handle_read_header(error);
                                });
        return 1;
    }

    bool check_precending_node(/*ID*/);

    void handle_message()
    {
        BOOST_LOG_TRIVIAL(debug) << get_name_tag() << "]handle_message()";
        if(packed_request_.unpack(readbuf_))
        {
            RequestPtr req = packed_request_.get_msg();
            auto commonheader = req->commonheader();

            /* //TODO: recheck if else is neccessarry */
            if(commonheader.t_flag()){
                if(handle_request(req)) return;
            } else {
                if(handle_response(req)) return;
            }

            BOOST_LOG_TRIVIAL(error) << get_name_tag() << "handle_message() failed";
            do_read_header();
        }
    }

    void handle_read_message(const boost::system::error_code& ec)
    {
        BOOST_LOG_TRIVIAL(debug) << get_name_tag() << "]handle_read_message()";
        if(!ec){
            handle_message();
        } else {
            BOOST_LOG_TRIVIAL(error) << "Error in handle_read_message: " << ec;
            do_read_header();
        }
    }

    void do_read_message(unsigned msg_len)
    {
        BOOST_LOG_TRIVIAL(debug) << get_name_tag() << "]do_read_message()";

        readbuf_.resize(HEADER_SIZE + msg_len);
        boost::asio::mutable_buffers_1 buf = boost::asio::buffer(&readbuf_[HEADER_SIZE], msg_len);
        /* boost::asio::async_read(socket_, buf, */
        /*         boost::bind(&session::handle_read_message, shared_from_this(), */
        /*             boost::asio::placeholders::error)); */
        boost::asio::async_read(socket_,
                                buf,
                                [me = shared_from_this()]
                                (boost::system::error_code const &error, std::size_t)
                                {
                                    me->handle_read_message(error);
                                });
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
            return handle_query(req);
        }

        if(request_type == "find_successor")
        {
             return handle_find_successor(req);
        }

        if(request_type == "get_predecessor")
        {
            return handle_get_predecessor(req);
        }

        if(request_type == "notify")
        {
            return handle_notify(req);
        }

        BOOST_LOG_TRIVIAL(debug) << get_name_tag() << "]Unknown Request Type: " << request_type;
        return false;
    }

    bool handle_response(RequestPtr req)
    {
        //determine request type
        //TODO: change get_request_type to more general name
        auto commonheader = req->commonheader();
        auto request_type = commonheader.request_type();
        BOOST_LOG_TRIVIAL(debug) << "[Session " << name << "handle_response() request_type: "
                                 << request_type;
        //handle request type
        if(request_type == "query")
        {
            return handle_query_response(req);
        }

        if (request_type == "find_successor")
        {
            return handle_find_successor_response(req);
        }

        if (request_type == "get_predecessor")
        {
            return handle_get_predecessor_response(req);
        }

        BOOST_LOG_TRIVIAL(debug) << get_name_tag() << "]Unknown Request Type";
        return false;
    }

    //SUCC AND PRE MUST BE ADDED TO FINGERTABLE
    //SUCC = FIGNER[1]
    //PRE  = FINGER[0]
    std::shared_ptr<Peer> closest_preceding_node(const std::string& id)
    {
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "closest_preceding_node()";

        auto self = routing_table_->get_self();
        auto finger = routing_table_->get_fingerTable();
        for(int i = finger->size() - 1; i >= 0; i--)
        {
            if(finger->at(i) == nullptr){
                break;
            }
            std::string finger_id = finger->at(i)->get_id();
            if(util::between(self->get_id(), finger_id, id)){
                return finger->at(i);
            }
        }
        std::cout << "RETURNING SELF" << std::endl;
        return self;
    }


    bool handle_query_response(RequestPtr req)
    {
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "handle_query_response";
        //TODO: extract peerinfo from Request and update own Peer information (Hash)

        //get own Peer object
        auto self = routing_table_->get_self();
        //set new ID
        self->set_id(req->peerinfo(0).peer_id());
        self->set_ip(req->peerinfo(0).peer_ip());
        self->set_port(req->peerinfo(0).peer_port());
        //get predecessor/successor object
        auto predecessor = routing_table_->get_predecessor();
        auto successor = routing_table_->get_successor();

        if(self->get_id() != "UNKNOWN"){
            promise_query.set_value(1);
            /* remote_find_successor(self); */
            return true;
        }

        return false;
    }

    bool handle_get_predecessor_response(RequestPtr req)
    {
        //TODO: error handling and foo
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "handle_get_predecessor_response";

        if(req->peerinfo_size() == 0){
            std::cout << "setting peer_to_wait_for" << std::endl;
            peer_to_wait_for = routing_table_->get_successor();
        } else {
            auto predecessor = req->peerinfo(0);
            peer_to_wait_for = std::make_shared<Peer>();
            peer_to_wait_for->setPeer(std::make_shared<PeerInfo>(predecessor));
        }

        promise.set_value(1);
        return true;
    }

    bool handle_find_successor_response(RequestPtr req)
    {
        if(req->peerinfo_size() != 1){
            BOOST_LOG_TRIVIAL(error) << get_name_tag() << "error: handle_find_successor_response";
            return false;
        }
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "handle_find_successor_response";
        auto new_id = req->peerinfo(0).peer_id();
        auto new_ip = req->peerinfo(0).peer_ip();
        auto new_port = req->peerinfo(0).peer_port();

        //TODO: dont set successor, let remote_find_suc return it!!!
        /* routing_table_->set_successor(std::make_shared<Peer>(new_id, new_ip)); */

        peer_to_wait_for = std::make_shared<Peer>(new_id, new_ip, new_port);
        if(peer_to_wait_for != nullptr){
            promise.set_value(1);
            std::cout << "promise was set!" << std::endl;
        }

        return true;
    }

    bool handle_find_successor(RequestPtr req)
    {
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::handle_find_successor";
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

        peerinfo->set_peer_id(peer->get_id());
        peerinfo->set_peer_ip(peer->get_ip());
        peerinfo->set_peer_port(peer->get_port());
        peerinfo->set_peer_rtt(peer->get_ip());
        peerinfo->set_peer_uptime(peer->get_ip());

        msg.init_header(false, 0, 0, "find_successor", "top_secret_id", VERSION);
        msg.pack(writebuf);
        /* boost::asio::write(socket_, boost::asio::buffer(writebuf)); */
        send(writebuf);
    }

    bool handle_query(RequestPtr req)
    {
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::handle_query";
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
        auto client_port = req->peerinfo(0).peer_port();
        auto client_hash = util::generate_sha256(client_ip, client_port);

        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "Query Received";
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "Client IP: " << client_ip;
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "New Client HASH: " << client_hash;
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "New Client port: " << client_port;

        Request request;

        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());
        auto message = msg.get_msg();
        auto peerinfo = message->add_peerinfo();

        peerinfo->set_peer_id(client_hash);
        peerinfo->set_peer_ip(client_ip);
        peerinfo->set_peer_port(client_port);
        peerinfo->set_peer_rtt(client_ip);
        peerinfo->set_peer_uptime(client_ip);

        msg.init_header(false, 0, 0, "query", "top_secret_id", VERSION);
        msg.pack(writebuf);
        send(writebuf);

        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "Sent query_response";
        do_read_header();

        return true;
    }

    bool handle_get_predecessor(RequestPtr req)
    {
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::handle_get_predecessor";

        Request request;

        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());
        auto message = msg.get_msg();

        auto predecessor = routing_table_->get_predecessor();

        if(predecessor != nullptr){
            auto peerinfo = message->add_peerinfo();
            //TODO: test this:
            /* peerinfo = predecessor->getPeer().get(); */
            peerinfo->set_peer_id(predecessor->get_id());
            peerinfo->set_peer_ip(predecessor->get_ip());
            peerinfo->set_peer_port(predecessor->get_port());
            peerinfo->set_peer_rtt(predecessor->get_ip());
            peerinfo->set_peer_uptime(predecessor->get_ip());
        } else {
            /* Peer peer; */
            /* peer.setPeer(std::make_shared<PeerInfo>(req->peerinfo(0))); */
            /* routing_table_->set_predecessor(std::make_shared<Peer>(peer)); */
        }

        msg.init_header(false, 0, 0, "get_predecessor", "top_secret_id", VERSION);
        msg.pack(writebuf);
        send(writebuf);

        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "Sent get_predecessor_response";

        return true;
    }

    bool handle_notify(RequestPtr req)
    {
        //TODO: SPAGETTHIIIIIIIIIIIIIii
        if(routing_table_->get_predecessor() == nullptr){
            Peer peer;
            peer.setPeer(std::make_shared<PeerInfo>(req->peerinfo(0)));
            routing_table_->set_predecessor(std::make_shared<Peer>(peer));
            return true;
        }

        auto predecessor = routing_table_->get_predecessor()->get_id();
        auto self = routing_table_->get_self()->get_id();
        Peer new_pre;
        new_pre.setPeer(std::make_shared<PeerInfo>(req->peerinfo(0)));
        auto new_pre_id = new_pre.get_id();

        if(util::between(predecessor, new_pre_id, self)){
            routing_table_->set_predecessor(std::make_shared<Peer>(new_pre));
        }
        return true;
    }

    bool send_response()
    {
        return false;
    }
    /**
     * A node MUST send a Query request to a discovered node before a join request.
     * Query request is used to determine overlay parameters such as overlay-ID,
     * peer-to-peer and hash algo, request routing method (recur vs iter).
     */
    bool query()
    {
        //TODO:: CHANGE TO USE MSG.PACK()!!!!!
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::query";
        //Creating PeerInfo with empty ID
        //This must be done before creating the CommonHeader because message_length
        //must be known when it gets encoded
        //TODO: still needed?
        uint32_t message_length(0);
        //Create a Request
        Request request;

        //TODO: rename
        std::shared_ptr<PeerInfo> peerinfo_ = routing_table_->get_self()->getPeer();

        //Set the message
        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());
        auto message = msg.get_msg();
        auto peerinfo = message->add_peerinfo();
        peerinfo->set_peer_id(peerinfo_->peer_id());
        peerinfo->set_peer_ip(peerinfo_->peer_ip());
        peerinfo->set_peer_port(peerinfo_->peer_port());
        peerinfo->set_peer_rtt(peerinfo_->peer_ip());
        peerinfo->set_peer_uptime(peerinfo_->peer_ip());
        msg.init_header(true, 0, 0, "query", "top_secret_id", VERSION);

        msg.pack(writebuf);
        send(writebuf);

        do_read_header();
        //TODO: wait till future_query is set

        return true;
    }

    void stabilize_internal()
    {
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::stabilize_internal";

        Request request;

        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());
        auto message = msg.get_msg();
        msg.init_header(true, 0, 0, "get_predecessor", "secret_id", VERSION);
        msg.pack(writebuf);
        send(writebuf);

        do_read_header();

        future.get();

        auto id = routing_table_->get_self()->get_id();
        std::cout << id << std::endl;
        auto id_succ = routing_table_->get_successor()->get_id();
        std::cout << id_succ << std::endl;
        auto id_succ_pre = peer_to_wait_for->get_id();
        std::cout << id_succ_pre << std::endl;

        if(util::between(id, id_succ_pre, id_succ)){
            routing_table_->set_successor(peer_to_wait_for);
        }
    }

    void notify_internal()
    {
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::notify_internal";

        Request request;
        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());

        auto message = msg.get_msg();
        auto self = routing_table_->get_self();
        auto peerinfo = message->add_peerinfo();

        peerinfo->set_peer_id(self->get_id());
        peerinfo->set_peer_ip(self->get_ip());
        peerinfo->set_peer_port(self->get_port());
        peerinfo->set_peer_rtt(self->get_ip());
        peerinfo->set_peer_uptime(self->get_ip());

        msg.init_header(true, 0, 0, "notify", "secret_id", VERSION);
        msg.pack(writebuf);
        send(writebuf);
    }

    void check_predecessor_internal()
    {
        BOOST_LOG_TRIVIAL(info) << get_name_tag() << "SESSION::check_predecessor_internal";

        Request request;
        std::vector<uint8_t> writebuf;
        PackedMessage<Request> msg = PackedMessage<Request>(std::make_shared<Request>());

        auto message = msg.get_msg();
        msg.init_header(true, 0, 0, "check_predecessor", "secret_id", VERSION);
        msg.pack(writebuf);
        send(writebuf);

        do_read_header();

        future.get();
    }

    std::string const get_name_tag()
    {
        std::stringstream str;
        str << "[SESSION " << name << "] ";
        return str.str();
    }

    std::string Put(int level_of_decryption);

    std::string Get(size_t hash);

    void do_write(std::size_t length);

    std::string name;
    RoutingPtr routing_table_;

    //used twice!!!
    std::shared_ptr<Peer> peer_to_wait_for;

    std::vector<uint8_t> readbuf_;
    PackedMessage<Request> packed_request_;

    //////NEW
    boost::asio::io_context io_context_;
    boost::asio::io_service& service_;
    tcp::socket socket_;
    boost::asio::io_service::strand write_strand_;
    boost::asio::io_service::strand read_strand_;
    std::deque<std::vector<uint8_t>> send_packet_queue;

    boost::promise<int> promise;
    boost::unique_future<int> future;

    boost::promise<int> promise_query;
    boost::unique_future<int> future_query;



};

#endif /* SESSION_H */