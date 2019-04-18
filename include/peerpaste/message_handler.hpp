#ifndef MESSAGE_HANDLER_HPP
#define MESSAGE_HANDLER_HPP

#include "peerpaste/session.hpp"
#include "peerpaste/message.hpp"
#include "peerpaste/request_object.hpp"
#include "peerpaste/aggregator.hpp"
#include "peerpaste/storage.hpp"
#include "peerpaste/concurrent_routing_table.hpp"
#include <functional>
#include <mutex>

class MessageHandler
{
public:
    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<Session> SessionPtr;
    typedef std::unique_ptr<RequestObject> RequestObjectUPtr;
    typedef std::shared_ptr<RequestObject> RequestObjectSPtr;
    typedef std::shared_ptr<Peer> PeerPtr;

    MessageHandler (short port,
                    std::shared_ptr<peerpaste::ConcurrentQueue<RequestObject>> queue_) :
                        send_queue_(queue_),
                        routing_table_(),
                        aggregator_(),
                        storage_(nullptr),
                        stabilize_flag_(false),
                        check_predecessor_flag_(false)
    {
        //TODO: setup self more accurate
        auto self_ip = "127.0.0.1";
        auto self_port = std::to_string(port);
        auto self_id = util::generate_sha256(self_ip, self_port);
        routing_table_.set_self(Peer(self_id, self_ip, self_port));

        storage_ = std::make_unique<Storage>(self_id);
    }

    MessageHandler (short port) :
                        routing_table_(),
                        aggregator_(),
                        storage_(nullptr),
                        stabilize_flag_(false),
                        check_predecessor_flag_(false)
    {
        //TODO: setup self more accurate
        auto self_ip = "127.0.0.1";
        auto self_port = std::to_string(port);
        auto self_id = util::generate_sha256(self_ip, self_port);
        routing_table_.set_self(Peer(self_id, self_ip, self_port));

        storage_ = std::make_unique<Storage>(self_id);
    }

    void init(std::shared_ptr<peerpaste::ConcurrentQueue<RequestObject>> queue__)
    {
        send_queue_ = queue__;
        Peer self;
        if(routing_table_.try_get_self(self)){
            routing_table_.set_successor(self);
        } else {
            std::cout << "MessageHandler could not init. Self was not set\n";
        }
    }

    void run()
    {
        /* handle_message(); */

        if(not stabilize_flag_){
            stabilize();
        }
        if(not check_predecessor_flag_){
            check_predecessor();
        }

        handle_timeouts();
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        run();
    }

    ~MessageHandler () {}

    //TODO: pass rvalue reference to only allow std::move()
    void push_to_write_queue(RequestObjectSPtr shared_transport_object)
    {
        auto is_request = shared_transport_object->get_message()->is_request();
        if(is_request){
            auto transaction_id = shared_transport_object->get_message()->get_transaction_id();
            //TODO: store ptrs in open_requests
            open_requests_[transaction_id] = shared_transport_object;
        }
        send_queue_->push(*shared_transport_object.get());
        routing_table_.print();
    }

    void handle_timeouts()
    {
        //for every obj in open_request
        for(auto iter = open_requests_.begin();
                 iter != open_requests_.end(); iter++){
            auto req_obj = iter->second;
            //if its still valid continue
            if(req_obj->is_valid()) continue;
            std::cout << "Dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd" << std::endl;
            std::cout << req_obj->get_request_type() << std::endl;
            //call handler_function with original obj
            req_obj->call(req_obj);
            //erase object from open_request
            open_requests_.erase(iter);
        }
    }

    void handle_request(RequestObjectUPtr transport_object)
    {
        auto request_type = transport_object->get_request_type();

        if(request_type == "query"){
            handle_query_request(std::move(transport_object));
            return;
        }
        if(request_type == "find_successor"){
            handle_find_successor_request(std::move(transport_object));
            return;
        }
        if(request_type == "get_predecessor"){
            handle_get_predecessor_request(std::move(transport_object));
            return;
        }
        if(request_type == "notify"){
            handle_notify(std::move(transport_object));
            return;
        }
        if(request_type == "check_predecessor"){
            handle_check_predecessor(std::move(transport_object));
            return;
        }
        if(request_type == "put"){
            handle_put_request(std::move(transport_object));
            return;
        }
        if(request_type == "get"){
            handle_get_request(std::move(transport_object));
            return;
        }

        std::cout << "UNKNOWN REQUEST TYPE: " << request_type << '\n';
    }

    void handle_get_request(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        auto data = message->get_data();

        auto response_message = message->generate_response();

        if(storage_->exists(data)){
            response_message->set_data(storage_->get(data));
        } else {
            response_message->set_data("Could not Find file, try again later");
        }

        response_message->generate_transaction_id();
        auto response = std::make_shared<RequestObject>(*transport_object);
        response->set_message(response_message);
        push_to_write_queue(response);
    }

    void handle_put_request(RequestObjectUPtr transport_object)
    {
        auto data = transport_object->get_message()->get_data();
        auto data_id = util::generate_sha256(data, "");
        storage_->put(data, data_id);

        auto message = transport_object->get_message();
        auto response_message = message->generate_response();
        response_message->set_data(data_id);
        response_message->generate_transaction_id();
        auto response = std::make_shared<RequestObject>(*transport_object);
        response->set_message(response_message);
        push_to_write_queue(response);
    }

    void handle_find_successor_request(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        if(message->get_peers().size() != 1){
            //TODO: handle invalid message here
        }

        auto id = message->get_peers().front().get_id();
        auto successor = find_successor(id);
        auto response_message = message->generate_response();
        if(successor == nullptr){
            //We have to use aggregator
            //to request successor remotely befor responding
            auto successor_predecessor = closest_preceding_node(id);
            auto new_request = std::make_shared<Message>();
            Header header(true, 0, 0, "find_successor", "", "", "");
            new_request->set_header(header);
            new_request->set_peers( { message->get_peers().front() } );
            new_request->generate_transaction_id();
            auto transaction_id = new_request->get_transaction_id();

            //Request that requests successor from another peer
            auto request = std::make_shared<RequestObject>();
            request->set_message(new_request);
            request->set_connection(successor_predecessor);

            //Response that will be sent back when request above
            //got answered
            auto response = std::make_shared<RequestObject>(*transport_object);
            response->set_message(message->generate_response());

            aggregator_.add_aggregat(response, { transaction_id });

            push_to_write_queue(request);
            return;
        }

        response_message->set_peers( { *successor.get() } );
        response_message->generate_transaction_id();

        auto response = std::make_shared<RequestObject>(*transport_object);
        response->set_message(response_message);

        push_to_write_queue(response);
    }

    std::shared_ptr<Peer> find_successor(const std::string& id)
    {
        Peer self;
        if(not routing_table_.try_get_self(self)){
            std::cout << "Cant find successor, self was not set" << std::endl;
            return nullptr;
        }

        //if peers are empty there is no successor
        if(routing_table_.size() == 0){
            auto predecessor = closest_preceding_node(id);
            if(predecessor->get_id() == self.get_id()){
                return std::make_shared<Peer>(self);
            }
            return nullptr;
        }

        auto self_id = self.get_id();
        Peer successor;
        if(not routing_table_.try_get_successor(successor)){
            std::cout << "Cant find successor, successor was not set" << std::endl;
            return nullptr;
        }
        auto succ_id = successor.get_id();
        if(util::between(self_id, id, succ_id) || id == succ_id)
        {
            return std::make_shared<Peer>(successor);
        } else {
            auto predecessor = closest_preceding_node(id);
            if(predecessor->get_id() == self.get_id()){
                return std::make_shared<Peer>(self);
            }
            return nullptr;
        }
    }

    //TODO: make priv
    std::shared_ptr<Peer> closest_preceding_node(const std::string& id)
    {
        Peer self;
        if(not routing_table_.try_get_self(self)){
            std::cout << "Cant find closest_preceding_node, self was not set" << std::endl;
            return nullptr;
        }
        auto peers = routing_table_.get_peers();
        for(int i = peers.size() - 1; i >= 0; i--)
        {
            std::string peer_id = peers.at(i).get_id();
            if(util::between(self.get_id(), peer_id, id)){
                return std::make_shared<Peer>(peers.at(i));
            }
        }
        return std::make_shared<Peer>(self);
    }

    void handle_query_request(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        if(message->get_peers().size() != 1){
            //TODO: handle invalid messages here
        }

        auto peer = message->get_peers().front();
        auto client_ip = transport_object->get_client_ip();
        auto client_port = peer.get_port();
        auto client_id = util::generate_sha256(client_ip, client_port);
        peer.set_ip(client_ip);
        peer.set_id(client_id);

        auto response_message = message->generate_response();
        response_message->add_peer(peer);
        response_message->generate_transaction_id();

        auto response = std::make_shared<RequestObject>(*transport_object);
        response->set_message(response_message);

        push_to_write_queue(response);
    }

    void handle_get_predecessor_request(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        if(message->get_peers().size() != 0){
            //TODO: handle_invalid_message!
            /* std::cout << "Invalid message handle_get_predecessor_request" << '\n'; */
        }

        auto response = message->generate_response();
        Peer predecessor;
        if(routing_table_.try_get_predecessor(predecessor)){
            response->set_peers( { predecessor } );
        } else {
            //TODO: send invalid message here, so that requestor knows that there
            //is no predecessor
            /* std::cout << "[MessageHandler] handle_get_predecessor_request():" */
                      /* << "not yet implemented" << '\n'; */
        }
        response->generate_transaction_id();

        auto response_object = std::make_shared<RequestObject>(*transport_object);
        response_object->set_message(response);
        push_to_write_queue(response_object);
    }

    void handle_response(RequestObjectUPtr transport_object)
    {
        //Get the CorrelationID to check if there is an OpenRequest matching
        //that ID
        auto correlational_id = transport_object->get_correlational_id();
        auto search = open_requests_.find(correlational_id);

        //if there is an open request
        if(search != open_requests_.end()){
            //Get the request object and erase it from the container
            auto request_object = open_requests_.at(correlational_id);
            open_requests_.erase(correlational_id);

            auto message = transport_object->get_message();

            //Check if the request has a handler function
            if(request_object->has_handler()){
                //call the handler function passing the response object
                request_object->call(std::move(transport_object));
            }
            //Get the response message
            //Check if there is an aggregat waiting for a message
            auto possible_request = aggregator_.add_aggregat(message);
            if(possible_request != nullptr){
                push_to_write_queue(possible_request);
            }

        } else {
            /* std::cout << "[MessageHandler] (handle_response) INVALID MSG " << '\n'; */
            //TODO: handle invalid message
        }
    }

    void stabilize()
    {
        Peer target;
        if(not routing_table_.try_get_successor(target)){
            return;
        }

        auto get_predecessor_msg = std::make_shared<Message>();
        get_predecessor_msg->set_header(Header(true, 0, 0, "get_predecessor", "", "", ""));
        get_predecessor_msg->generate_transaction_id();
        auto transaction_id = get_predecessor_msg->get_transaction_id();

        auto handler = std::bind(&MessageHandler::handle_stabilize,
                                 this,
                                 std::placeholders::_1);

        auto request = std::make_shared<RequestObject>();
        request->set_handler(handler);
        request->set_message(get_predecessor_msg);
        request->set_connection(std::make_shared<Peer>(target));
        push_to_write_queue(request);

        stabilize_flag_ = true;
    }

    void check_predecessor()
    {
        Peer target;
        if(not routing_table_.try_get_predecessor(target)){
            return;
        }

        auto notify_message = std::make_shared<Message>();
        notify_message->set_header(Header(true, 0, 0, "check_predecessor", "", "", ""));
        notify_message->generate_transaction_id();
        auto transaction_id = notify_message->get_transaction_id();

        auto handler = std::bind(&MessageHandler::handle_check_predecessor_response,
                                 this,
                                 std::placeholders::_1);

        auto request = std::make_shared<RequestObject>();
        request->set_message(notify_message);
        request->set_handler(handler);
        request->set_connection(std::make_shared<Peer>(target));
        push_to_write_queue(request);
        check_predecessor_flag_ = true;
    }

    void handle_check_predecessor_response(RequestObjectUPtr transport_object)
    {
        check_predecessor_flag_ = false;
        if(!transport_object->get_message()->is_request()){
            return;
        }
        routing_table_.reset_predecessor();
    }

    void handle_check_predecessor(RequestObjectUPtr transport_object)
    {
        //Generate and push response
        auto message = transport_object->get_message();
        auto response = message->generate_response();
        response->generate_transaction_id();

        auto response_object = std::make_shared<RequestObject>(*transport_object);
        response_object->set_message(response);
        push_to_write_queue(response_object);
    }

    void notify()
    {
        //TODO: add better abstracted checking if succ/pre/whatever is initialized
        Peer self;
        if(not routing_table_.try_get_self(self)) return;
        if(self.get_id() == "") return;

        Peer target;
        if(not routing_table_.try_get_successor(target)){
            return;
        }

        auto notify_message = std::make_shared<Message>();
        notify_message->set_header(Header(true, 0, 0, "notify", "", "", ""));
        notify_message->add_peer(self);
        notify_message->generate_transaction_id();
        auto transaction_id = notify_message->get_transaction_id();

        auto handler = std::bind(&MessageHandler::handle_notify_response,
                                 this,
                                 std::placeholders::_1);

        auto request = std::make_shared<RequestObject>();
        request->set_message(notify_message);
        request->set_handler(handler);
        request->set_connection(std::make_shared<Peer>(target));
        push_to_write_queue(request);
    }

    void handle_notify_response(RequestObjectUPtr transport_object)
    {
        return;
    }

    void handle_notify(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        if(message->get_peers().size() != 1){
            //TODO: handle invalid msg
            std::cout << "handle notify invalid message" << '\n';
            return;
        }

        auto notify_peer = message->get_peers().front();
        if(not routing_table_.has_predecessor()){
            routing_table_.set_predecessor(notify_peer);
            return;
        }

        Peer predecessor;
        Peer self;

        if(not routing_table_.try_get_predecessor(predecessor)){
            std::cout << "cant handle notify: no predecessor set" << std::endl;
        }
        if(not routing_table_.try_get_self(self)){
            std::cout << "cant handle notify: self not set" << std::endl;
        }

        auto predecessor_id = predecessor.get_id();
        auto self_id = self.get_id();
        auto notify_peer_id = notify_peer.get_id();

        if(util::between(predecessor_id, notify_peer_id, self_id)){
            routing_table_.set_predecessor(notify_peer);
        }

        //Generate and push response
        auto response = message->generate_response();
        response->generate_transaction_id();

        auto response_object = std::make_shared<RequestObject>(*transport_object);
        response_object->set_message(response);
        push_to_write_queue(response_object);
    }

    /*
     * THere are many edge cases not handled yet. for example:
     * if freshly joined and the own hash is the new successor of the file hash
     * you want to get, then it is not found
     *
     */
    void get(const std::string& data)
    {
        routing_table_.wait_til_valid();

        auto get_request_message = std::make_shared<Message>();
        Header get_request_message_header(true, 0, 0, "get", "", "", "");
        get_request_message->set_header(get_request_message_header);
        get_request_message->set_data(data);
        get_request_message->generate_transaction_id();

        auto get_request = std::make_shared<RequestObject>();
        auto get_request_handler = std::bind(&MessageHandler::handle_get_response,
                                                              this,
                                                              std::placeholders::_1);
        get_request->set_handler(get_request_handler);
        get_request->set_message(get_request_message);

        auto succ = find_successor(data);

        //if no successor is found
        if(succ == nullptr){
            //We have to use agggregat
            auto succ_prede = closest_preceding_node(data);
            auto new_request = std::make_shared<Message>();
            Header header(true, 0, 0, "find_successor", "", "", "");
            new_request->set_header(header);
            new_request->set_peers({ Peer(data, "", "") });
            new_request->generate_transaction_id();
            auto transaction_id = new_request->get_transaction_id();

            auto request = std::make_shared<RequestObject>();
            request->set_message(new_request);
            request->set_connection(succ_prede);
            aggregator_.add_aggregat(get_request, { transaction_id });
            push_to_write_queue(request);
            return;
        }
        get_request->set_connection(succ);
        push_to_write_queue(get_request);
    }

    void handle_get_response(RequestObjectUPtr transport_object)
    {
        std::cout << "GET RESPONSE HANDLER" << std::endl;

        std::cout << transport_object->get_message()->get_data() << std::endl;
    }

    void put(const std::string& data)
    {
        routing_table_.wait_til_valid();

        //generate data id
        auto data_id = util::generate_sha256(data, "");

        //TODO: generate put request here, holding the data string
        //then check if we know the successor allready.
        //if that is the case just send the put request
        //if it is not the case:
        //generate find successor request for the data_id
        //create an aggregat which waits till successor is found
        //and then sends the put request

        //also the message object needs the be aware of the new data string
        //and the protobuf message converter too
        auto put_request_message = std::make_shared<Message>();
        Header put_request_message_header(true, 0,0, "put", "", "", "");
        put_request_message->set_header(put_request_message_header);
        put_request_message->set_data(data);
        put_request_message->generate_transaction_id();

        auto put_request = std::make_shared<RequestObject>();

        auto put_request_handler = std::bind(&MessageHandler::handle_put_response,
                                                            this,
                                                            std::placeholders::_1);

        put_request->set_handler(put_request_handler);
        put_request->set_message(put_request_message);

        //try to find successor
        auto succ = find_successor(data_id);
        /* std::cout << "Find Succ of data_id:" << std::endl; */
        /* std::cout << succ->get_id() << std::endl; */

        //if no successor is found
        if(succ == nullptr){
            //We have to use agggregat
            auto succ_prede = closest_preceding_node(data_id);
            auto new_request = std::make_shared<Message>();
            Header header(true, 0, 0, "find_successor", "", "", "");
            new_request->set_header(header);
            new_request->set_peers({ Peer(data_id, "", "") });
            new_request->generate_transaction_id();
            auto transaction_id = new_request->get_transaction_id();

            auto request = std::make_shared<RequestObject>();
            request->set_message(new_request);
            request->set_connection(succ_prede);
            aggregator_.add_aggregat(put_request, { transaction_id });
            push_to_write_queue(request);
            return;
        }
        put_request->set_connection(succ);
        push_to_write_queue(put_request);
    }

    void handle_put_response(RequestObjectUPtr transport_object)
    {
        /* std::cout << "###############################" << std::endl; */
        /* std::cout << "######HANDLE_PUT_RESPONSE######" << std::endl; */
        /* std::cout << "###############################" << std::endl; */
        std::cout << "Paste is stored. Get it using this is: " << std::endl;
        std::cout << transport_object->get_message()->get_data() << std::endl;
    }

    void join(std::string address, std::string port)
    {
        //Generate Query
        MessagePtr query_request = std::make_shared<Message>();

        Header query_header(true, 0, 0, "query", "", "", "");
        Peer self;
        if(not routing_table_.try_get_self(self)){
            std::cout << "Cant join, self not set" << std::endl;
        }

        query_request->set_header(query_header);
        query_request->add_peer(self);
        query_request->generate_transaction_id();
        auto transaction_id = query_request->get_transaction_id();

        auto target = std::make_shared<Peer>("", address, port);

        auto handler = std::bind(&MessageHandler::handle_query_response,
                                                            this,
                                                            std::placeholders::_1);

        auto request = std::make_shared<RequestObject>();
        request->set_handler(handler);
        request->set_message(query_request);
        request->set_connection(target);

        //Generate Find Successor Request
        MessagePtr find_successor_request = std::make_shared<Message>();
        Header find_succressor_header(true, 0, 0, "find_successor", "", "", "");
        find_successor_request->set_header(find_succressor_header);
        /* find_successor_request->generate_transaction_id(); */

        auto successor_handler = std::bind(&MessageHandler::handle_join_response,
                                                            this,
                                                            std::placeholders::_1);

        auto successor_request = std::make_shared<RequestObject>();
        successor_request->set_handler(successor_handler);
        successor_request->set_message(find_successor_request);
        successor_request->set_connection(target);

        //Add succsessor request as aggregat, so that it gets send
        //as soon as the query_response was handled
        aggregator_.add_aggregat(successor_request, { transaction_id });
        push_to_write_queue(request);
    }

    void handle_join_response(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        if(message->get_peers().size() != 1){
            //TODO: handle invalid message
        }

        auto successor = message->get_peers().front();
        routing_table_.set_successor(successor);
    }

    void handle_query_response(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        if(message->get_peers().size() == 1){
            //TODO: make  message peers to be shared_ptrs
            auto peer = message->get_peers().front();
            routing_table_.set_self(peer);
        } else {
            /* std::cout << "WRONG MESSAGE SIZE() handle_query_response" << '\n'; */
            //TODO: handle invalid message
        }
    }

    void handle_stabilize(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();

        Peer self;
        if(not routing_table_.try_get_self(self)){
            std::cout << "Cant handle stabilize: self not set" << std::endl;
            return;
        }

        if(message->get_peers().size() != 1 || message->is_request()){
            //TODO: handle invalid message
            routing_table_.set_successor(self);
            stabilize_flag_ = false;
            notify();
            return;
        }

        auto successors_predecessor = message->get_peers().front();

        Peer successor;
        if(not routing_table_.try_get_successor(successor)){
            std::cout << "Cant handle stabilize: successor not set" << std::endl;
            return;
        }

        if(util::between(self.get_id(),
                         successors_predecessor.get_id(),
                         successor.get_id())){
            routing_table_.set_successor(successors_predecessor);
        }
        notify();
        stabilize_flag_ = false;
    }

    /* const RoutingTable get_routing_table() */
    /* { */
    /*     return routing_table_; */
    /* } */

private:

    peerpaste::ConcurrentRoutingTable<Peer> routing_table_;
    std::shared_ptr<peerpaste::ConcurrentQueue<RequestObject>>  send_queue_;
    std::unique_ptr<Storage> storage_;

    //TODO:
    //std::map<string, RequestObject>
    //RequestObject contains: function ptr,
    //                        message (with timestamp!),
    //                        Peer to send the request
    /* std::map<std::string, std::function<void(MessagePtr)>> open_requests_; */
    //TODO: delete objects when they got handled
    std::map<std::string, RequestObjectSPtr> open_requests_;

    Aggregator aggregator_;

    mutable std::mutex mutex_;

    bool stabilize_flag_;
    bool check_predecessor_flag_;
};
#endif /* MESSAGE_HANDLER_HPP */
