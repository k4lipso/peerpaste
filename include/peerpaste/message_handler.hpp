#ifndef MESSAGE_HANDLER_HPP
#define MESSAGE_HANDLER_HPP

#include "peerpaste/message.hpp"
#include "peerpaste/request_object.hpp"
#include "peerpaste/aggregator.hpp"
#include "peerpaste/storage.hpp"
#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/concurrent_request_handler.hpp"
#include <functional>
#include <mutex>

class MessageHandler
{
public:
    typedef std::shared_ptr<Message> MessagePtr;
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

        std::cout << "OWN ID: " << self_id << std::endl;
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

        std::cout << "OWN ID: " << self_id << std::endl;
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

        open_requests_.handle_timeouts();
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
            open_requests_.set(transaction_id, shared_transport_object);
        }
        send_queue_->push(*shared_transport_object.get());
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
        if(request_type == "get_predecessor_and_succ_list"){
            get_predecessor_and_succ_list_request(std::move(transport_object));
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
        if(request_type == "get_successor_list"){
            handle_get_successor_list_request(std::move(transport_object));
            return;
        }
        if(request_type == "get_self_and_successor_list"){
            handle_get_self_and_successor_list_request(std::move(transport_object));
            return;
        }
        if(request_type == "put"){
            handle_put_request(std::move(transport_object));
            return;
        }
        if(request_type == "store"){
            handle_store(std::move(transport_object));
            return;
        }
        if(request_type == "get"){
            handle_get_request(std::move(transport_object));
            return;
        }
        if(request_type == "get_internal"){
            handle_get_internal(std::move(transport_object));
            return;
        }


        std::cout << "UNKNOWN REQUEST TYPE: " << request_type << '\n';
    }

    void handle_get_request(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        auto response_msg = message->generate_response();
        auto response = std::make_shared<RequestObject>(*transport_object.get());
        response->set_message(response_msg);
        auto data = message->get_data();
        //get self to check if find_successor returned self
        Peer self;
        if(not routing_table_.try_get_self(self)){
            std::cout << "cant handle_put request, self not defined" << std::endl;
            return;
        }
        auto succ = find_successor(data);
        if(succ == nullptr){
            auto find_succ_req_msg = std::make_shared<Message>(
                    Message::create_request("get"));
            find_succ_req_msg->set_data(data);
            auto transaction_id = find_succ_req_msg->generate_transaction_id();
            auto find_succ_req = std::make_unique<RequestObject>();
            find_succ_req->set_message(find_succ_req_msg);
            find_succ_req->set_connection(closest_preceding_node(data));

            aggregator_.add_aggregat(std::move(response), { transaction_id });
            push_to_write_queue(std::move(find_succ_req));
            return;

        } else if(succ->get_id() == self.get_id()){
            if(storage_->exists(data)){
                response_msg->set_data(storage_->get(data));
            } else {
                response_msg->set_data("Could not Find file, try again later");
            }
            response_msg->generate_transaction_id();
            push_to_write_queue(response);
            return;
        }
        auto get_internal_message = std::make_shared<Message>(
                Message::create_request("get_internal"));
        get_internal_message->set_data(data);
        auto transaction_id = get_internal_message->generate_transaction_id();
        auto get_internal_request = std::make_unique<RequestObject>();
        get_internal_request->set_message(get_internal_message);
        get_internal_request->set_connection(succ);

        aggregator_.add_aggregat(std::move(response), { transaction_id });
        push_to_write_queue(std::move(get_internal_request));
    }

    void handle_get_internal(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        auto data = message->get_data();
        auto response_msg = message->generate_response();
        auto response = std::make_shared<RequestObject>(*transport_object.get());
        if(storage_->exists(data)){
            response_msg->set_data(storage_->get(data));
        } else {
            response_msg->set_data("Could not Find file, try again later");
        }
        response->set_message(response_msg);
        push_to_write_queue(response);
    }

    void handle_put_request(RequestObjectUPtr transport_object)
    {
        //get msg and generate response
        auto msg = transport_object->get_message();
        auto response_msg = msg->generate_response();
        auto response = std::make_shared<RequestObject>(*transport_object.get());
        response->set_message(response_msg);
        //get data and generate id
        auto data = msg->get_data();
        auto data_id = util::generate_sha256(data, "");
        //get self to check if find_successor returned self
        Peer self;
        if(not routing_table_.try_get_self(self)){
            std::cout << "cant handle_put request, self not defined" << std::endl;
            return;
        }
        //try to find successor
        auto succ = find_successor(data_id);
        //if no successor is found
        if(succ == nullptr){
            //we have to forward put request to another peer
            //We have to use agggregat
            auto new_put_request = std::make_unique<RequestObject>(*transport_object.get());
            new_put_request->get_message()->set_data(data);
            auto transaction_id = new_put_request->get_message()->generate_transaction_id();
            auto succ_prede = closest_preceding_node(data_id);
            new_put_request->set_connection(succ_prede);

            aggregator_.add_aggregat(std::move(response), { transaction_id });
            push_to_write_queue(std::move(new_put_request));
            return;
        } else if(succ->get_id() == self.get_id()) {
            //we are the right peer to store the data!
            storage_->put(data, data_id);
            response_msg->set_data(data_id);
            /* response_msg->generate_transaction_id(); */
            push_to_write_queue(response);
            return;
        }
        //we know the successor so we forward the data
        auto store_message = std::make_shared<Message>(
                Message::create_request("store"));
        store_message->set_data(data);
        auto transaction_id = store_message->generate_transaction_id();
        auto new_put_request = std::make_unique<RequestObject>();
        new_put_request->set_message(store_message);
        new_put_request->set_connection(succ);

        aggregator_.add_aggregat(std::move(response), { transaction_id });

        push_to_write_queue(std::move(new_put_request));
    }

    void handle_store(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        auto data = message->get_data();
        auto data_id = util::generate_sha256(data, "");
        auto response_msg = message->generate_response();
        auto response = std::make_shared<RequestObject>(*transport_object.get());
        response->set_message(response_msg);
        storage_->put(data, data_id);
        response_msg->set_data(data_id);
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
            auto new_request = std::make_shared<Message>(
                                Message::create_request("find_successor",
                                                        { message->get_peers().front() }));
            auto transaction_id = new_request->generate_transaction_id();

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

    //TODO: ADD TESTING!
    std::shared_ptr<Peer> find_successor(const std::string& id)
    {
        Peer self;
        if(not routing_table_.try_get_self(self)){
            std::cout << "Cant find successor, self was not set" << std::endl;
            return nullptr;
        }

        //if peers are empty there is no successor
        if(routing_table_.size() == 0){
            std::cout << "ALLALALALALA" << std::endl;
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

    void get_predecessor_and_succ_list_request(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        if(message->get_peers().size() != 0){
            //TODO: handle_invalid_message!
            /* std::cout << "Invalid message handle_get_predecessor_request" << '\n'; */
        }

        auto response = message->generate_response();
        Peer predecessor;
        if(routing_table_.try_get_predecessor(predecessor)){
            auto peers = routing_table_.get_peers();
            peers.insert(peers.begin(), predecessor);
            response->set_peers(peers);
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
        RequestObjectSPtr request_object;
        if(open_requests_.try_find_and_erase(correlational_id, request_object)){
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
            std::cout << "[MessageHandler] (handle_response) INVALID MSG " << '\n';
            transport_object->get_message()->print();
            //TODO: handle invalid message
        }
    }

    //TODO: add stabilization according to "how to make chord correct", using the extended succ list
    void stabilize()
    {
        Peer target;
        if(not routing_table_.try_get_successor(target)){
            return;
        }

        auto get_predecessor_msg = std::make_shared<Message>(
                            Message::create_request("get_predecessor_and_succ_list"));
        auto transaction_id = get_predecessor_msg->get_transaction_id();

        auto handler = std::bind(&MessageHandler::handle_stabilize,
                                 this,
                                 std::placeholders::_1);

        auto request = std::make_shared<RequestObject>();
        request->set_handler(handler);
        request->set_message(get_predecessor_msg);
        request->set_connection(std::make_shared<Peer>(target));
        assert(not request->is_session());
        push_to_write_queue(request);
        assert(not request->is_session());

        stabilize_flag_ = true;
    }

    void check_predecessor()
    {
        Peer target;
        if(not routing_table_.try_get_predecessor(target)){
            return;
        }

        auto notify_message = std::make_shared<Message>(
                            Message::create_request("check_predecessor"));
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

        auto notify_message = std::make_shared<Message>(
                            Message::create_request("notify", { self }));
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

        //TODO: if we have a predecessor node we should check if its alive!
        //check the rectify operation in how_to_make_chord_correct.pdf
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
    /* void get(const std::string& data) */
    /* { */
        /* routing_table_.wait_til_valid(); */

        /* auto get_request_message = std::make_shared<Message>(); */
        /* Header get_request_message_header(true, 0, 0, "get", "", "", ""); */
        /* get_request_message->set_header(get_request_message_header); */
        /* get_request_message->set_data(data); */
        /* get_request_message->generate_transaction_id(); */

        /* auto get_request = std::make_shared<RequestObject>(); */
        /* auto get_request_handler = std::bind(&MessageHandler::handle_get_response, */
        /*                                                       this, */
        /*                                                       std::placeholders::_1); */
        /* get_request->set_handler(get_request_handler); */
        /* get_request->set_message(get_request_message); */

        /* auto succ = find_successor(data); */

        /* //if no successor is found */
        /* if(succ == nullptr){ */
        /*     //We have to use agggregat */
        /*     auto succ_prede = closest_preceding_node(data); */
        /*     auto new_request = std::make_shared<Message>(); */
        /*     Header header(true, 0, 0, "find_successor", "", "", ""); */
        /*     new_request->set_header(header); */
        /*     new_request->set_peers({ Peer(data, "", "") }); */
        /*     auto transaction_id = new_request->generate_transaction_id(); */

        /*     auto request = std::make_shared<RequestObject>(); */
        /*     request->set_message(new_request); */
        /*     request->set_connection(succ_prede); */
        /*     aggregator_.add_aggregat(get_request, { transaction_id }); */
        /*     push_to_write_queue(request); */
        /*     return; */
        /* } */
        /* get_request->set_connection(succ); */
        /* push_to_write_queue(get_request); */
    /* } */

    void handle_get_response(RequestObjectUPtr transport_object)
    {
        std::cout << "GET RESPONSE HANDLER" << std::endl;

        std::cout << transport_object->get_message()->get_data() << std::endl;
    }

    void get(const std::string& ip,
             const std::string& port,
             const std::string& data)
    {
        auto get_request_message = std::make_shared<Message>(
                                            Message::create_request("get"));
        get_request_message->set_data(data);
        get_request_message->generate_transaction_id();

        auto get_request = std::make_shared<RequestObject>();

        auto get_request_handler = std::bind(&MessageHandler::handle_get_response,
                                                            this,
                                                            std::placeholders::_1);

        get_request->set_handler(get_request_handler);
        get_request->set_message(get_request_message);

        get_request->set_connection(std::make_shared<Peer>(Peer("", ip, port)));
        push_to_write_queue(get_request);
    }

    void put(const std::string& ip,
             const std::string& port,
             const std::string& data)
    {
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
        auto put_request_message = std::make_shared<Message>(
                                            Message::create_request("put"));
        put_request_message->set_data(data);
        put_request_message->generate_transaction_id();

        auto put_request = std::make_shared<RequestObject>();

        auto put_request_handler = std::bind(&MessageHandler::handle_put_response,
                                                            this,
                                                            std::placeholders::_1);

        put_request->set_handler(put_request_handler);
        put_request->set_message(put_request_message);

        put_request->set_connection(std::make_shared<Peer>(Peer("", ip, port)));
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
        Peer self;
        if(not routing_table_.try_get_self(self)){
            std::cout << "Cant join, self not set" << std::endl;
        }

        //Generate Query
        auto query_request = std::make_shared<Message>(
                            Message::create_request("query", { self }));
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
        auto find_successor_request = std::make_shared<Message>(
                            Message::create_request("find_successor"));

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

        //Generate get_successor_list request
        auto get_successor_list_request = std::make_shared<Message>(
                            Message::create_request("get_successor_list"));

        auto successor_handler = std::bind(&MessageHandler::handle_get_successor_list_response,
                                                            this,
                                                            std::placeholders::_1);

        auto successor_list_request = std::make_shared<RequestObject>();
        successor_list_request->set_handler(successor_handler);
        successor_list_request->set_message(get_successor_list_request);
        successor_list_request->set_connection(std::make_shared<Peer>(successor));

        push_to_write_queue(successor_list_request);
    }

    void handle_get_successor_list_response(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        auto new_succ_list = message->get_peers();
        routing_table_.replace_after_successor(new_succ_list);
    }

    void handle_set_pred_succ_list(RequestObjectUPtr transport_object)
    {
        if(transport_object->is_request()){
            notify();
            stabilize_flag_ = false;
            return;
        }
        auto message = transport_object->get_message();
        auto new_succ_list = message->get_peers();
        routing_table_.replace_successor_list(new_succ_list);
        notify();
        stabilize_flag_ = false;

    }

    void handle_get_self_and_successor_list_request(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();

        auto response_message = message->generate_response();
        auto succ_list = routing_table_.get_peers();

        Peer self;
        if(not routing_table_.try_get_self(self)){
            std::cout << "couldnt get self in handle_get_self_and_successor_list_request\n";
        }
        succ_list.insert(succ_list.begin(), self);
        response_message->set_peers(succ_list);
        response_message->generate_transaction_id();

        auto response = std::make_shared<RequestObject>(*transport_object);
        response->set_message(response_message);

        push_to_write_queue(response);
    }

    void handle_get_successor_list_request(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();

        auto response_message = message->generate_response();
        response_message->set_peers(routing_table_.get_peers());
        response_message->generate_transaction_id();

        auto response = std::make_shared<RequestObject>(*transport_object);
        response->set_message(response_message);

        push_to_write_queue(response);
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

        if(message->get_peers().size() < 1 || message->is_request()){
            //TODO: handle invalid message
            //TODO: CHange to pop_front()
            routing_table_.pop_front();
            stabilize_flag_ = false;
            notify();
            return;
        }

        //get peers, they consists of: {predecessor, successor_list}
        auto peers = message->get_peers();
        //get the predecessor and erase him aftwerwards
        auto successors_predecessor = peers.front();
        if(peers.size() > 1){
            peers.erase(peers.begin());
        }
        //get own successor
        Peer successor;
        if(not routing_table_.try_get_successor(successor)){
            std::cout << "Cant handle stabilize: successor not set" << std::endl;
            return;
        }

        //insert the peer at beginning of his succ list to make it our own succ list
        peers.insert(peers.begin(), successor);
        routing_table_.replace_successor_list(peers);


        //check if successors predecessor will be our new successor
        if(util::between(self.get_id(),
                         successors_predecessor.get_id(),
                         successor.get_id())){
            auto get_successor_list_request = std::make_shared<Message>(
                            Message::create_request("get_self_and_successor_list"));

            auto successor_handler = std::bind(&MessageHandler::handle_set_pred_succ_list,
                                                                this,
                                                                std::placeholders::_1);

            auto successor_list_request = std::make_shared<RequestObject>();
            successor_list_request->set_handler(successor_handler);
            successor_list_request->set_message(get_successor_list_request);
            successor_list_request->set_connection(std::make_shared<Peer>(successors_predecessor));

            push_to_write_queue(successor_list_request);
        } else {
            notify();
            stabilize_flag_ = false;
        }
    }

    std::tuple<Peer, Peer, Peer> get_routing_information()
    {
        Peer pre, self, succ;
        routing_table_.try_get_predecessor(pre);
        routing_table_.try_get_self(self);
        routing_table_.try_get_successor(succ);
        return std::make_tuple(pre, self, succ);
    }

private:

    peerpaste::ConcurrentRoutingTable<Peer> routing_table_;
    peerpaste::ConcurrentRequestHandler<RequestObjectSPtr> open_requests_;
    std::shared_ptr<peerpaste::ConcurrentQueue<RequestObject>>  send_queue_;
    std::unique_ptr<Storage> storage_;
    Aggregator aggregator_;

    mutable std::mutex mutex_;

    bool stabilize_flag_;
    bool check_predecessor_flag_;
};
#endif /* MESSAGE_HANDLER_HPP */
