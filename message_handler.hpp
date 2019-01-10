#ifndef MESSAGE_HANDLER_HPP
#define MESSAGE_HANDLER_HPP

#include "session.hpp"
#include "message.hpp"
#include "message_queue.hpp"
#include "routing_table.hpp"
#include "request_object.hpp"
#include <functional>

class MessageHandler
{
public:
    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<Session> SessionPtr;

    MessageHandler (boost::asio::io_context& io_context, short port) :
                        routing_table_(),
                        service_(io_context),
                        timer_(io_context, boost::asio::chrono::seconds(1))
    {
        routing_table_.get_self()->set_port(std::to_string(port));
        message_queue_ = MessageQueue::GetInstance();
        run();
    }

    ~MessageHandler () {}

    void run()
    {
        handle_message();
        //TODO: check if request are outdated. then retry or cancel
        //look at TODO at bottom (RequestObj)

        timer_.expires_at(timer_.expiry() + boost::asio::chrono::seconds(1));
        timer_.async_wait(boost::bind(&MessageHandler::run, this));
    }

    void handle_message()
    {
        bool message_queue_is_empty = message_queue_->empty();
        if(message_queue_is_empty){
            return;
        }

        auto message = message_queue_->front().first;
        auto session = message_queue_->front().second;
        message_queue_->pop_front();
        message->print();

        //TODO: handle invalid messages here

        if(message->is_request()){
            handle_request(message, session);
        } else {
            handle_response(message, session);
        }
        handle_message();
    }

    void handle_request(MessagePtr message, SessionPtr session)
    {
        std::cout << "handle_request" << std::endl;
        auto request_type = message->get_request_type();

        if(request_type == "query"){
            handle_query_request(message, session);
        }

    }

    void handle_query_request(MessagePtr message, SessionPtr session)
    {
        std::cout << "handle_query_request" << std::endl;
        if(message->get_peers().size() != 1){
            //TODO: handle invalid messages here
        }

        auto peer = message->get_peers().front();
        auto client_ip = session->get_client_ip();
        auto client_port = peer.get_port();
        auto client_id = util::generate_sha256(client_ip, client_port);
        peer.set_ip(client_ip);
        peer.set_id(client_id);

        auto response_message = message->generate_response();
        response_message->add_peer(peer);
        response_message->generate_transaction_id();
        session->write(response_message);

        session->read();
    }

    void handle_response(MessagePtr message, SessionPtr session)
    {
        std::cout << "handle_response" << std::endl;

        auto correlational_id = message->get_correlational_id();
        auto search = open_requests_.find(correlational_id);
        if(search != open_requests_.end()){
            auto request_object = open_requests_.at(correlational_id);
            request_object.call(message);
             /* auto handler = open_requests_.at(correlational_id); */
             /* handler(message); */
        } else {
            //TODO: handle invalid message
        }
    }

    void query(std::string address, std::string port)
    {
        MessagePtr query_request = std::make_shared<Message>();

        Header query_header(true, 0, 0, "query", "", "", "");
        auto self = routing_table_.get_self();

        query_request->set_header(query_header);
        query_request->add_peer(*self.get());
        query_request->generate_transaction_id();
        auto transaction_id = query_request->get_transaction_id();

        auto query_handler = std::make_shared<Session>(service_);
        query_handler->write_to(query_request, address, port);
        query_handler->read();

        auto handler = std::bind(&MessageHandler::handle_query_response,
                                                            this,
                                                            std::placeholders::_1);

        RequestObject request;
        request.set_handler(handler);
        request.set_message(query_request);
        request.set_connection(query_handler);
        open_requests_[transaction_id] = request;
    }

    void handle_query_response(MessagePtr message)
    {
        std::cout << "QUERY RESPONSE HANDLER" << std::endl;
        if(message->get_peers().size() == 1){
            //TODO: make  message peers to be shared_ptrs
            auto peer = std::make_shared<Peer>(message->get_peers().front());
            routing_table_.set_self(peer);
            std::cout << "### SELF:" << std::endl;
            routing_table_.get_self()->print();
        } else {
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

    //TODO:
    //std::map<string, RequestObject>
    //RequestObject contains: function ptr,
    //                        message (with timestamp!),
    //                        Peer to send the request
    /* std::map<std::string, std::function<void(MessagePtr)>> open_requests_; */
    std::map<std::string, RequestObject> open_requests_;
};
#endif /* MESSAGE_HANDLER_HPP */
