#ifndef MESSAGE_HANDLER_HPP
#define MESSAGE_HANDLER_HPP

#include "session.hpp"
#include "message.hpp"
#include "message_queue.hpp"
#include "write_queue.hpp"
#include "routing_table.hpp"
#include "request_object.hpp"
#include "aggregator.hpp"
#include "storage.hpp"
#include <functional>

class MessageHandler
{
public:
    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<Session> SessionPtr;
    typedef std::unique_ptr<RequestObject> RequestObjectUPtr;
    typedef std::shared_ptr<RequestObject> RequestObjectSPtr;
    typedef std::shared_ptr<Peer> PeerPtr;

    MessageHandler (boost::asio::io_context& io_context, short port) :
                        routing_table_(),
                        aggregator_(),
                        storage_(nullptr)
    {
        //TODO: setup self more accurate
        routing_table_.get_self()->set_port(std::to_string(port));
        routing_table_.get_self()->set_ip("127.0.0.1");
        routing_table_.get_self()->set_id(util::generate_sha256("127.0.0.1",
                                                                std::to_string(port)));
        /* storage_ = Storage(std::string(routing_table_.get_self()->get_id())); */
        storage_ = std::make_unique<Storage>(routing_table_.get_self()->get_id());

        message_queue_ = MessageQueue::GetInstance();
        write_queue_ = WriteQueue::GetInstance();
    }

    void init()
    {
        routing_table_.set_successor(routing_table_.get_self());
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
        write_queue_->push_back(shared_transport_object);
    }

    void handle_message()
    {
        const bool message_queue_is_empty = message_queue_->empty();
        if(message_queue_is_empty){
            return;
        }

        auto transport_object = message_queue_->front();
        message_queue_->pop_front();

        const bool is_request = transport_object->get_message()->is_request();
        transport_object->get_message()->print();

        //TODO: handle invalid messages here

        if(is_request){
            handle_request(std::move(transport_object));
        } else {
            handle_response(std::move(transport_object));
        }
        handle_message();
    }

    void handle_request(RequestObjectUPtr&& transport_object)
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
        if(request_type == "put"){
            handle_put_request(std::move(transport_object));
            return;
        }
        if(request_type == "get"){
            handle_get_request(std::move(transport_object));
            return;
        }

        /* std::cout << "UNKNOWN REQUEST TYPE: " << request_type << '\n'; */
    }

    void handle_get_request(RequestObjectUPtr&& transport_object)
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

    void handle_put_request(RequestObjectUPtr&& transport_object)
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

    void handle_find_successor_request(RequestObjectUPtr&& transport_object)
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

    const std::shared_ptr<Peer> find_successor(const std::string& id)
    {
        auto self = routing_table_.get_self();

        //if peers are empty there is no successor
        if(routing_table_.get_peers().size() == 0){
            auto predecessor = closest_preceding_node(id);
            if(predecessor->get_id() == self->get_id()){
                return self;
            }
            return nullptr;
        }

        auto self_id = self->get_id();
        auto successor = routing_table_.get_successor();
        auto succ_id = successor->get_id();
        if(util::between(self_id, id, succ_id) || id == succ_id)
        {
            return successor;
        } else {
            auto predecessor = closest_preceding_node(id);
            if(predecessor->get_id() == self->get_id()){
                return self;
            }
            return nullptr;
        }
    }

    //TODO: make priv
    std::shared_ptr<Peer> closest_preceding_node(const std::string& id)
    {
        auto self = routing_table_.get_self();
        auto peers = routing_table_.get_peers();
        for(int i = peers.size() - 1; i >= 0; i--)
        {
            if(peers.at(i) == nullptr){
                //TODO: continue instead?
                break;
            }
            std::string peer_id = peers.at(i)->get_id();
            if(util::between(self->get_id(), peer_id, id)){
                return peers.at(i);
            }
        }
        return self;
    }

    void handle_query_request(RequestObjectUPtr&& transport_object)
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

    void handle_get_predecessor_request(RequestObjectUPtr&& transport_object)
    {
        auto message = transport_object->get_message();
        if(message->get_peers().size() != 0){
            //TODO: handle_invalid_message!
            /* std::cout << "Invalid message handle_get_predecessor_request" << '\n'; */
        }

        auto response = message->generate_response();
        auto predecessor = routing_table_.get_predecessor();
        if(predecessor != nullptr){
            response->set_peers( { *predecessor.get() } );
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

    void handle_response(RequestObjectUPtr&& transport_object)
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

        auto get_predecessor_msg = std::make_shared<Message>();
        get_predecessor_msg->set_header(Header(true, 0, 0, "get_predecessor", "", "", ""));
        get_predecessor_msg->generate_transaction_id();
        auto transaction_id = get_predecessor_msg->get_transaction_id();

        auto target = routing_table_.get_successor();
        if(target == nullptr){
            return;
        }

        auto handler = std::bind(&MessageHandler::handle_stabilize,
                                 this,
                                 std::placeholders::_1);

        auto request = std::make_shared<RequestObject>();
        request->set_handler(handler);
        request->set_message(get_predecessor_msg);
        request->set_connection(target);
        push_to_write_queue(request);
    }

    void notify()
    {
        //TODO: add better abstracted checking if succ/pre/whatever is initialized
        if(routing_table_.get_self()->get_id() == ""){
            /* std::cout << "Notifi: error, no id for self" << '\n'; */
            return;
        }

        auto target = routing_table_.get_successor();
        if(target == nullptr){
            /* std::cout << "Notifi: error, no successor found" << '\n'; */
            return;
        }


        auto notify_message = std::make_shared<Message>();
        notify_message->set_header(Header(true, 0, 0, "notify", "", "", ""));
        notify_message->add_peer(*routing_table_.get_self().get());
        notify_message->generate_transaction_id();
        auto transaction_id = notify_message->get_transaction_id();

        auto handler = std::bind(&MessageHandler::handle_notify_response,
                                 this,
                                 std::placeholders::_1);

        auto request = std::make_shared<RequestObject>();
        request->set_message(notify_message);
        request->set_handler(handler);
        request->set_connection(target);
        push_to_write_queue(request);
    }

    void handle_notify_response(const RequestObjectUPtr transport_object)
    {
        std::cout << "handle_notify_response" << '\n';
        return;
    }

    void handle_notify(RequestObjectUPtr&& transport_object)
    {
        auto message = transport_object->get_message();
        if(message->get_peers().size() != 1){
            //TODO: handle invalid msg
            /* std::cout << "handle notify invalid message" << '\n'; */
            return;
        }

        auto notify_peer = std::make_shared<Peer>(message->get_peers().front());
        if(routing_table_.get_predecessor() == nullptr){
            routing_table_.set_predecessor(notify_peer);
            return;
        }

        auto predecessor_id = routing_table_.get_predecessor()->get_id();
        auto self_id = routing_table_.get_self()->get_id();
        auto notify_peer_id = notify_peer->get_id();

        if(util::between(predecessor_id, notify_peer_id, self_id)){
            routing_table_.set_predecessor(notify_peer);
        }
        routing_table_.print();
    }

    /*
     * THere are many edge cases not handled yet. for example:
     * if freshly joined and the own hash is the new successor of the file hash
     * you want to get, then it is not found
     *
     */
    void get(const std::string& data)
    {
        while(!routing_table_.is_valid()){
            std::cout << "invalid" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

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

    void handle_get_response(const RequestObjectUPtr transport_object)
    {
        std::cout << "GET RESPONSE HANDLER" << std::endl;

        std::cout << transport_object->get_message()->get_data() << std::endl;
    }

    void put(const std::string& data)
    {
        while(!routing_table_.is_valid()){
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
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

    void handle_put_response(const RequestObjectUPtr transport_object)
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
        auto self = routing_table_.get_self();

        query_request->set_header(query_header);
        query_request->add_peer(*self.get());
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

    void handle_join_response(const RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        if(message->get_peers().size() != 1){
            //TODO: handle invalid message
        }

        auto successor = std::make_shared<Peer>(message->get_peers().front());
        routing_table_.set_successor(successor);
        routing_table_.print();
    }

    void handle_query_response(const RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();
        if(message->get_peers().size() == 1){
            //TODO: make  message peers to be shared_ptrs
            auto peer = std::make_shared<Peer>(message->get_peers().front());
            routing_table_.set_self(peer);
            routing_table_.get_self()->print();
        } else {
            /* std::cout << "WRONG MESSAGE SIZE() handle_query_response" << '\n'; */
            //TODO: handle invalid message
        }
    }

    void handle_stabilize(const RequestObjectUPtr transport_object)
    {
        auto message = transport_object->get_message();

        if(message->get_peers().size() != 1){
            //TODO: handle invalid message
            /* std::cout << "invalid message size at handle_stabilize" << '\n'; */
            return;
        }

        auto self = routing_table_.get_self();
        auto successor = routing_table_.get_successor();
        auto successors_predecessor = message->get_peers().front();

        if(util::between(self->get_id(),
                         successors_predecessor.get_id(),
                         successor->get_id())){
            routing_table_.set_successor(std::make_shared<Peer>(successors_predecessor));
        }
        routing_table_.print();
    }

    const RoutingTable get_routing_table()
    {
        return routing_table_;
    }

    void handle_find_successor_response(MessagePtr message);
    void handle_check_predecessor_response(MessagePtr message);
    void handle_get_predecessor_response(MessagePtr message);

private:

    RoutingTable routing_table_;
    std::unique_ptr<Storage> storage_;
    std::shared_ptr<MessageQueue> message_queue_;
    std::shared_ptr<WriteQueue> write_queue_;

    //TODO:
    //std::map<string, RequestObject>
    //RequestObject contains: function ptr,
    //                        message (with timestamp!),
    //                        Peer to send the request
    /* std::map<std::string, std::function<void(MessagePtr)>> open_requests_; */
    //TODO: delete objects when they got handled
    std::map<std::string, RequestObjectSPtr> open_requests_;

    Aggregator aggregator_;
};
#endif /* MESSAGE_HANDLER_HPP */
