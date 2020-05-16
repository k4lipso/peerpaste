#pragma once

#include <functional>
#include <future>

#include "peerpaste/message.hpp"
#include "peerpaste/request_object.hpp"
#include "peerpaste/aggregator.hpp"
#include "peerpaste/storage.hpp"
#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/concurrent_request_handler.hpp"
#include "peerpaste/message_factory.hpp"

#include "peerpaste/thread_pool.hpp"
#include "peerpaste/observer_base.hpp"
#include "peerpaste/messaging_base.hpp"

#include "peerpaste/messages/notify.hpp"
#include "peerpaste/messages/check_predecessor.hpp"


class MessageHandler : public ObserverBase
{
public:
    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::unique_ptr<RequestObject> RequestObjectUPtr;
    typedef std::shared_ptr<RequestObject> RequestObjectSPtr;
    typedef std::shared_ptr<Peer> PeerPtr;

    MessageHandler(short port)
      : routing_table_()
      , storage_(nullptr)
      , aggregator_()
      , stabilize_flag_(false)
      , check_predecessor_flag_(false)
      , thread_pool_(0)
      , message_factory_{ &routing_table_ }
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
            util::log(error, "MessageHandler could not init. Self was not set");
        }
    }

    virtual void HandleNotification(const RequestObject& request_object) override
    {
      send_queue_->push(request_object);
    }

    virtual void HandleNotification(const RequestObject& request_object, HandlerObject<HandlerFunction> handler) override
    {
      active_handlers_.insert(handler);
      send_queue_->push(request_object);
    }

    virtual void HandleNotification() override
    {
      active_messages_.clean();
      active_handlers_.erase_if([](auto& Handler) { return !Handler.is_valid(); });
    }

    void run(){
        run_thread_.emplace_back( [this]{ run_chord_internal(); } );
        //run_thread_.emplace_back( [=]{ run_paste_internal(); } );
    }

    void stop()
    {
        running_ = false;
        for(auto& t : run_thread_){
            t.join();
        }
    }

    void run_chord_internal()
    {
        // routing_table_.print();

        if(not stabilize_flag_){
            stabilize();
        }
        if(not check_predecessor_flag_){
            check_predecessor();
        }

        open_requests_deprecated_.handle_timeouts();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        if(running_){
            run_chord_internal();

        }
    }

    void run_paste_internal()
    {
        stabilize_storage();
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));

        if (running_) {
          run_paste_internal();
        }
    }

    ~MessageHandler () {}

    void push_to_write_queue(RequestObjectSPtr shared_transport_object)
    {
        auto is_request = shared_transport_object->get_message()->is_request();
        if(is_request){
            auto transaction_id = shared_transport_object->get_message()->get_transaction_id();
            //TODO: store ptrs in open_requests
            open_requests_deprecated_.set(transaction_id, shared_transport_object);
        }
        send_queue_->push(*shared_transport_object.get());
    }

  void handle_request(RequestObjectUPtr transport_object)
  {
    std::shared_ptr<MessagingBase> message_object = message_factory_.create_from_request(*transport_object);

    if(message_object)
    {
      message_object->Attach(this);
      thread_pool_.submit(message_object);
      active_messages_.push_back(std::move(message_object));
      return;
    }

    //DEPRECATED

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
    if (request_type == "backup") {
      handle_backup(std::move(transport_object));
      return;
    }
    util::log(warning, "Unknown Request Type: " + request_type);
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
            util::log(warning, "cant handle_put request, self not defined");
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
            util::log(warning, "cant handle_put request, self not defined");
            return;
        }
        //try to find successor
        auto succ = find_successor(data_id);
        //if no successor is found
        if(succ == nullptr){
            //we have to find the successor
            //and then send a "store" request to the successor.
            //to accomplish this we have we have to build two nested aggregats
            //one that is sending the store request after the successor is found
            //and one that is sending the actual put response
            //when the store request was successfull
            auto find_succ_msg =
                std::make_shared<Message>(Message::create_request(
                                              "find_successor", { Peer(data_id, "", "")}));
            auto transaction_id_find_succ = find_succ_msg->generate_transaction_id();
            auto find_succ_req = std::make_shared<RequestObject>();
            find_succ_req->set_message(find_succ_msg);
            find_succ_req->set_connection(closest_preceding_node(data_id));

            auto store_msg = std::make_shared<Message>(
                Message::create_request("store"));
            store_msg->set_data(data);
            auto transaction_id_store = store_msg->generate_transaction_id();
            auto store_request = std::make_unique<RequestObject>();
            store_request->set_message(store_msg);

            aggregator_.add_aggregat(std::move(store_request), { transaction_id_find_succ });
            aggregator_.add_aggregat(std::move(response), { transaction_id_store });
            push_to_write_queue(find_succ_req);
            return;
        } else if(succ->get_id() == self.get_id()) {
            //we are the right peer to store the data!
            storage_->put(data, data_id);
            response_msg->set_data(data_id);
            push_to_write_queue(response);
            return;
        }
        //we know the successor so we forward the data
        auto store_msg = std::make_shared<Message>(
                Message::create_request("store"));
        store_msg->set_data(data);
        auto transaction_id = store_msg->generate_transaction_id();
        auto store_request = std::make_unique<RequestObject>();
        store_request->set_message(store_msg);
        store_request->set_connection(succ);

        aggregator_.add_aggregat(std::move(response), { transaction_id });

        push_to_write_queue(std::move(store_request));
    }

    void handle_store(RequestObjectUPtr transport_object)
    {
        util::log(debug, "handle store");
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
            util::log(warning, "Cant find successor, self was not set");
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
            util::log(warning, "Cant find successor, successor was not set");
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
            util::log(warning, "Cant find closest_preceding_node, self was not set");
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
            util::log(warning, "Invalid message handle_get_predecessor_req");
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

        const auto handler_object = active_handlers_.get_and_erase(correlational_id);
        if(handler_object.has_value())
        {
          if(auto parent = handler_object.value().lock())
          {
            handler_object.value().handler_(*transport_object);
          }
          else
          {
            util::log(info, "weak_ptr expired");
          }

          return;
        }

        RequestObjectSPtr request_object;
        if(open_requests_deprecated_.try_find_and_erase(correlational_id, request_object)){
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
            util::log(warning, "Invalid Message");
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
                            Message::create_request(
                                "get_predecessor_and_succ_list"));
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

    void stabilize_storage()
    {
        util::log(debug, "stabilize storage");
        auto files = storage_->get_map();
        for(const auto& file : files){
            auto succ = find_successor(file.first);
            Peer self;
            if(not routing_table_.try_get_self(self)){
                util::log(debug, "self was not set");
                continue;
            }
            Peer predecessor;
            if (not routing_table_.try_get_predecessor(predecessor)) {
              util::log(debug, "predecessor was not set");
              continue;
            }

            if (util::between(predecessor.get_id(), file.first, self.get_id())) {
                Peer successor;
                if (not routing_table_.try_get_successor(successor)) {
                  util::log(debug, "successor was not set");
                  continue;
                }

                auto backup_msg = std::make_shared<Message>(
                    Message::create_request("backup"));
                backup_msg->set_data(file.first);
                auto transaction_id = backup_msg->get_transaction_id();
                auto handler = std::bind(&MessageHandler::handle_backup_response,
                                         this,
                                         std::placeholders::_1);
                auto backup_request = std::make_shared<RequestObject>();
                backup_request->set_handler(handler);
                backup_request->set_message(backup_msg);
                backup_request->set_connection(std::make_shared<Peer>(successor));
                util::log(debug, "sending backup request");
                push_to_write_queue(backup_request);
            } else {
                if(not storage_->is_valid(file.first)){
                    storage_->remove(file.first);
                }
            }
        }
    }

    void handle_backup(RequestObjectUPtr transport_object)
    {
        util::log(debug, "handle_backup");
        auto data = transport_object->get_message()->get_data();
        std::string response_str = "NO";
        if (not storage_->exists(data)) {
          storage_->refresh_validity(data);
          response_str = data;
        }
        auto response_msg = transport_object->get_message()->generate_response();
        Peer self;
        if(not routing_table_.try_get_self(self)){
            util::log(debug, "self was not set");
            return;
        }
        response_msg->add_peer(self);
        auto response =
            std::make_shared<RequestObject>(*transport_object.get());
        response->set_message(response_msg);
        response_msg->set_data(response_str);
        push_to_write_queue(response);
    }

    void handle_backup_response(RequestObjectUPtr transport_object)
    {
        //placeholder
        util::log(debug, "handle backup response");
        auto data = transport_object->get_message()->get_data();
        if(data == "NO"){ return; }
        if(not storage_->exists(data)){
            util::log(debug, "Data does not exists");
            return;
        }
        auto store_msg =
            std::make_shared<Message>(Message::create_request("store"));
        store_msg->set_data(storage_->get(data));
        auto transaction_id = store_msg->get_transaction_id();

        auto store_request = std::make_shared<RequestObject>();
        store_request->set_message(store_msg);
        store_request->set_connection(std::make_shared<Peer>(
            transport_object->get_message()->get_peers().front()));
        push_to_write_queue(store_request);
    }

    void check_predecessor()
    {
      auto CheckPreMessage = std::make_shared<peerpaste::message::CheckPredecessor>(&routing_table_, &check_predecessor_flag_);

      CheckPreMessage->Attach(this);
      thread_pool_.submit(CheckPreMessage);
      active_messages_.push_back(std::move(CheckPreMessage));
    }

    void notify()
    {
      auto NotifyMessage = std::make_shared<peerpaste::message::Notification>(&routing_table_);

      NotifyMessage->Attach(this);
      thread_pool_.submit(NotifyMessage);
      active_messages_.push_back(std::move(NotifyMessage));
    }

    [[deprecated]]
    void handle_get_response(RequestObjectUPtr transport_object)
    {
        std::cout << "GET RESPONSE HANDLER" << std::endl;

        std::cout << transport_object->get_message()->get_data() << std::endl;
    }

    std::future<std::string> get(const std::string& ip,
             const std::string& port,
             const std::string& data)
    {
        auto get_request_message = std::make_shared<Message>(
                                            Message::create_request("get"));
        get_request_message->set_data(data);
        auto transaction_id = get_request_message->generate_transaction_id();

        auto get_request = std::make_shared<RequestObject>();

        get_request->set_message(get_request_message);
        get_request->set_connection(std::make_shared<Peer>(Peer("", ip, port)));


        //create dummy request for storing promise
        //TODO: this is only needed to be able to set the promise value on return.
        //maybe add a possibility to create response handler functions that also have access
        //to the original request? right now a normal response handler has only access to the
        //response object, not the original request that was sendet (and that is holding a promise value)
        auto dummy_message = std::make_shared<Message>(
                                Message::create_request("get_dummy"));
        dummy_message->set_data("");
        auto dummy_request = std::make_shared<RequestObject>();
        dummy_request->set_message(dummy_message);
        //create and set promise
        auto promise_ = std::make_shared<std::promise<std::string>>();
        auto future_ = promise_->get_future();
        dummy_request->set_promise(promise_);
        aggregator_.add_aggregat(dummy_request, { transaction_id });

        push_to_write_queue(get_request);
        return future_;
    }

    std::future<std::string> put(const std::string& ip,
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
        auto transaction_id = put_request_message->generate_transaction_id();

        auto put_request = std::make_shared<RequestObject>();

        put_request->set_message(put_request_message);
        put_request->set_connection(std::make_shared<Peer>(Peer("", ip, port)));

        //TODO: same todo as in get() function
        auto dummy_message = std::make_shared<Message>(
                                Message::create_request("put_dummy"));
        dummy_message->set_data(data_id);
        auto dummy_request = std::make_shared<RequestObject>();
        dummy_request->set_message(dummy_message);
        //create and set promise
        auto promise_ = std::make_shared<std::promise<std::string>>();
        auto future_ = promise_->get_future();
        dummy_request->set_promise(promise_);
        aggregator_.add_aggregat(dummy_request, { transaction_id });

        push_to_write_queue(put_request);
        return future_;
    }

    [[deprecated]]
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
            util::log(warning, "Cant join, self not set");
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
            util::log(warning, "Could not get self in handle_get_self_and_successor_list_request");
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
            util::log(warning, "Wrong Message_Size");
            //TODO: handle invalid message
        }
    }

    void handle_stabilize(RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        //check if message is still request, which would mean it timed out
        if(message->is_request()){
            util::log(debug, "handle_stabilize aborted, transport obj is request");
        }


        Peer self;
        if(not routing_table_.try_get_self(self)){
            util::log(warning, "Cant handle stabilize: self not set");
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
            util::log(warning, "Cant handle stabilize: successor not set");
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

    ThreadPool thread_pool_;
private:

    peerpaste::ConcurrentRoutingTable<Peer> routing_table_;
    peerpaste::deprecated::ConcurrentRequestHandler<RequestObjectSPtr> open_requests_deprecated_;
    std::shared_ptr<peerpaste::ConcurrentQueue<RequestObject>>  send_queue_;
    std::unique_ptr<Storage> storage_;
    Aggregator aggregator_;

    mutable std::mutex mutex_;

    bool stabilize_flag_;
    std::atomic<bool> check_predecessor_flag_;
    bool running_ = true;
    std::vector<std::thread> run_thread_;

    peerpaste::message::MessageFactory message_factory_;
    peerpaste::ConcurrentDeque<std::shared_ptr<MessagingBase>> active_messages_;
    peerpaste::ConcurrentSet<HandlerObject<HandlerFunction>, std::less<>> active_handlers_;
};
