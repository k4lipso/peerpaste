#ifndef MESSAGE_HANDLER_HPP
#define MESSAGE_HANDLER_HPP

#include "session.hpp"
#include "message.hpp"
#include "message_queue.hpp"
#include "write_queue.hpp"
#include "routing_table.hpp"
#include "request_object.hpp"
#include "aggregator.hpp"
#include <functional>

class MessageHandler
{
public:
    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<Session> SessionPtr;
    typedef std::shared_ptr<RequestObject> RequestObjectPtr;

    MessageHandler (boost::asio::io_context& io_context, short port) :
                        routing_table_(),
                        aggregator_(),
                        service_(io_context),
                        timer_(io_context, boost::asio::chrono::seconds(1))
    {
        //TODO: setup self more accurate
        routing_table_.get_self()->set_port(std::to_string(port));
        routing_table_.get_self()->set_ip("127.0.0.1");
        routing_table_.get_self()->set_id(util::generate_sha256("127.0.0.1",
                                                                std::to_string(port)));
        message_queue_ = MessageQueue::GetInstance();
        write_queue_ = WriteQueue::GetInstance();
        run();
    }

    ~MessageHandler () {}

    void run()
    {
        handle_message();
        handle_write_queue();
        //TODO: check if request are outdated. then retry or cancel
        //look at TODO at bottom (RequestObj)
        //TODO: check send_queue

        timer_.expires_at(timer_.expiry() + boost::asio::chrono::seconds(1));
        timer_.async_wait(boost::bind(&MessageHandler::run, this));
    }

    void handle_write_queue()
    {
        bool write_queue_is_empty = write_queue_->empty();
        if(write_queue_is_empty){
            return;
        }

        //TODO: store as ptr in write_queue!
        auto request = std::make_shared<RequestObject>(*write_queue_->front());
        write_queue_->pop_front();
        auto message = request->get_message();
        auto is_request = message->is_request();

        if(request->is_session()){
            auto session = request->get_session();
            session->write(message);
            if(is_request){
                session->read();
            }
        } else {
            //TODO: this is to session/boost specific
            //when using a test_socket or something like that this code
            //should be more abstract so that the message_handler does not need
            //to know what kind of object sends the data somewhere
            auto peer = request->get_peer();
            auto write_handler = std::make_shared<Session>(service_);
            write_handler->write_to(message, peer->get_ip(), peer->get_port());
            if(is_request){
                write_handler->read();
            }

        }

        if(is_request){
            auto transaction_id = message->get_transaction_id();
            //TODO: store ptrs in open_requests
            open_requests_[transaction_id] = request;
        }
    }

    void push_to_write_queue(const RequestObjectPtr transport_object)
    {
        auto is_request = transport_object->get_message()->is_request();
        if(is_request){
            auto transaction_id = transport_object->get_message()->get_transaction_id();
            //TODO: store ptrs in open_requests
            open_requests_[transaction_id] = transport_object;
        }
        write_queue_->push_back(transport_object);
        std::cout << "PUSH TO WRITE SIZE: " << open_requests_.size() << std::endl;
    }

    void handle_message()
    {
        bool message_queue_is_empty = message_queue_->empty();
        if(message_queue_is_empty){
            return;
        }

        auto transport_object = message_queue_->front();
        auto message = transport_object->get_message();
        message_queue_->pop_front();

        //TODO: handle invalid messages here

        if(message->is_request()){
            handle_request(transport_object);
        } else {
            handle_response(transport_object);
        }
        handle_message();
    }

    void handle_request(RequestObjectPtr transport_object)
    {
        std::cout << "handle_request" << std::endl;
        auto request_type = transport_object->get_request_type();

        if(request_type == "query"){
            handle_query_request(transport_object);
            return;
        }
        if(request_type == "find_successor"){
            handle_find_successor_request(transport_object);
            return;
        }

        std::cout << "UNKNOWN REQUEST TYPE: " << request_type << std::endl;
    }

    void handle_find_successor_request(const RequestObjectPtr transport_object)
    {
        std::cout << "handle_find_successor_request()" << std::endl;
        auto message = transport_object->get_message();
        if(message->get_peers().size() != 1){
            //TODO: handle invalid message here
        }

        auto id = message->get_peers().front().get_id();
        auto successor = find_successor(id);
        auto response_message = message->generate_response();
        if(successor == nullptr){
            std::cout << "SUC IS NULLPTr" << std::endl;
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
                std::cout << "find_successor() returns self" << std::endl;
                return self;
            }
            std::cout << "closest_preceding_node: " << predecessor->get_id() << std::endl;
            std::cout << "self id: " << self->get_id() << std::endl;
            return nullptr;
        }

        auto self_id = self->get_id();
        auto successor = routing_table_.get_successor();
        auto succ_id = successor->get_id();
        if(util::between(self_id, id, succ_id) || id == succ_id)
        {
            std::cout << "RETURNED SUCC in find_successor" << std::endl;
            return successor;
        } else {
            auto predecessor = closest_preceding_node(id);
            if(predecessor->get_id() == self->get_id()){
                std::cout << "PREDECESSOR IS SELF" << std::endl;
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
        std::cout << "RETURNING SELF" << std::endl;
        return self;
    }

    void handle_query_request(const RequestObjectPtr transport_object)
    {
        std::cout << "handle_query_request" << std::endl;
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

    void handle_response(const RequestObjectPtr transport_object)
    {
        std::cout << "handle_response" << std::endl;

        auto correlational_id = transport_object->get_correlational_id();
        auto search = open_requests_.find(correlational_id);
        if(search != open_requests_.end()){
            auto request_object = open_requests_.at(correlational_id);
            open_requests_.erase(correlational_id);
            /* auto request_object = open_requests_.extract(correlational_id).value; */
            if(request_object->has_handler()){
                request_object->call(transport_object);
            } else {
                std::cout << "handle response: no handler specified" << '\n';
            }

            auto message = transport_object->get_message();
            auto possible_request = aggregator_.add_aggregat(message);
            if(possible_request != nullptr){
                std::cout << "PUSHING TO DA WRITE QEUEE" << std::endl;
                push_to_write_queue(possible_request);
            }

        } else {
            std::cout << "INVALID MSG (handle_response)" << std::endl;
            //TODO: handle invalid message
        }
    }

    std::string query(std::string address, std::string port)
    {
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
        push_to_write_queue(request);
        return transaction_id;
    }

    void join(std::string address, std::string port)
    {
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

        aggregator_.add_aggregat(successor_request, { transaction_id });
        push_to_write_queue(request);
    }

    void handle_join_response(const RequestObjectPtr transport_object)
    {
        auto message = transport_object->get_message();
        std::cout << "HANDLE JOIN RESPONSE MOTHER FUCKER" << std::endl;
        if(message->get_peers().size() != 1){
            //TODO: handle invalid message
        }

        auto successor = std::make_shared<Peer>(message->get_peers().front());
        routing_table_.set_successor(successor);
        routing_table_.print();
    }

    void handle_query_response(const RequestObjectPtr transport_object)
    {
        auto message = transport_object->get_message();
        std::cout << "QUERY RESPONSE HANDLER" << std::endl;
        if(message->get_peers().size() == 1){
            //TODO: make  message peers to be shared_ptrs
            auto peer = std::make_shared<Peer>(message->get_peers().front());
            routing_table_.set_self(peer);
            std::cout << "### SELF:" << std::endl;
            routing_table_.get_self()->print();
        } else {
            std::cout << "WRONG MESSAGE SIZE()" << std::endl;
            //TODO: handle invalid message
        }
    }

    void handle_find_successor_response(MessagePtr message);
    void handle_notify_response(MessagePtr message);
    void handle_check_predecessor_response(MessagePtr message);
    void handle_get_predecessor_response(MessagePtr message);

private:

    boost::asio::io_service& service_;
    boost::asio::steady_timer timer_;

    RoutingTable routing_table_;
    std::shared_ptr<MessageQueue> message_queue_;
    std::shared_ptr<WriteQueue> write_queue_;

    //TODO:
    //std::map<string, RequestObject>
    //RequestObject contains: function ptr,
    //                        message (with timestamp!),
    //                        Peer to send the request
    /* std::map<std::string, std::function<void(MessagePtr)>> open_requests_; */
    //TODO: delete objects when they got handled
    std::map<std::string, RequestObjectPtr> open_requests_;

    Aggregator aggregator_;
};
#endif /* MESSAGE_HANDLER_HPP */
