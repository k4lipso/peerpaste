#ifndef MESSAGE_DISPATCHER
#define CONSUMER_DISPATCHER

#include "peerpaste/concurrent_queue.hpp"
#include "peerpaste/request_object.hpp"
#include "peerpaste/message_converter.hpp"
#include "peerpaste/message.hpp"
#include "peerpaste/message_handler.hpp"
#include "peerpaste/session.hpp"
#include "peerpaste/boost_session.hpp"
#include "peerpaste/observer_base.hpp"

#include <utility>
#include <memory>
#include <vector>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace peerpaste {

using MsgPtr = std::unique_ptr<Message>;
using SessionPtr = std::shared_ptr<Session>;
using MsgBufPair = std::pair<std::vector<uint8_t>, SessionPtr>;
using MsgPair = std::pair<MsgPtr, SessionPtr>;

class MessageDispatcher
{
public:
    //TODO: should init both queues and provide getter() functions to get shared_ptrs of them
    MessageDispatcher (std::shared_ptr<MessageHandler> msg_handler)
        : msg_handler_(msg_handler),
          input_queue_(std::make_shared<ConcurrentQueue<MsgBufPair>>()),
          output_queue_(std::make_shared<ConcurrentQueue<RequestObject>>())
    {}

    std::shared_ptr<ConcurrentQueue<MsgBufPair>> get_receive_queue(){
        return input_queue_;
    }

    std::shared_ptr<ConcurrentQueue<RequestObject>> get_send_queue(){
        return output_queue_;
    }

    boost::asio::io_context& get_context()
    {
        return io_context_;
    }

    /*
     * Should be called to start handling messages. Stoped by calling stop()
     * Pops Msg from Queue, converts it and dispatches it to the MessageHandler
     */
    void run()
    {
        thread_pool_deprecated_.emplace_back( [this]{ run_internal(); } );
        thread_pool_deprecated_.emplace_back( [=]{ run_send_internal(); } );

        //TODO: add thread count instead of 4
        for(int i=0; i < 4; ++i)
        {
            asio_pool_.emplace_back( [=]{ io_context_.run(); } );
        }
    }

    void stop()
    {
        run_ = false;
        io_context_.stop();
        for(auto& thread_pool : thread_pool_deprecated_){
            util::log(debug, "joining thread of thread_pool_deprecated_");
            thread_pool.join();
        }
        while(not io_context_.stopped()){
            util::log(debug, "wait for io_context to stop");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        for(auto& asio_pool : asio_pool_){
            util::log(debug, "joining thread of asio_pool_");
            asio_pool.join();
        }
    }

    void join()
    {
        for(auto it = thread_pool_deprecated_.begin(); it < thread_pool_deprecated_.end(); it++){
            it->join();
        }
        for(auto it2 = asio_pool_.begin(); it2 < asio_pool_.end(); it2++){
            it2->join();
        }
    }

    void send_routing_information()
    {
        thread_pool_deprecated_.emplace_back( [=]{ send_routing_information_internal(); } );
    }

    void run_internal()
    {
        ProtobufMessageConverter converter;
        while(run_){
            //Get unique_ptr to MsgPair from queue
            auto msg_pair = input_queue_->wait_for_and_pop();
            if(msg_pair == nullptr) continue;
            //Convert msg_buffer into Message object
            //TODO: handle failure on MessageFromSerialized!!!
            auto converted_message = converter.MessageFromSerialized(
                                        std::get<0>(std::move(*msg_pair.get())));
            //create RequestObject
            auto data_object = std::make_unique<RequestObject>();
            //set msg
            data_object->set_message(std::move(converted_message));
            //set conn
            data_object->set_connection(std::move(std::get<1>(*msg_pair.get())));
            //dispatch
            dispatch(std::move(data_object));
        }
    }

    void run_send_internal()
    {
        ProtobufMessageConverter converter;
        while(run_){
            //Get unique_ptr to RequestObject from queue
            auto send_object = output_queue_->wait_for_and_pop();
            if(send_object == nullptr) continue;
            //get the message to send
            auto message = send_object->get_message();
            auto message_is_request = message->is_request();

            //convert message to buf
            auto message_buf = converter.SerializedFromMessage(message);
            std::vector<boost::uint8_t> encoded_buf;
            //header size used in session is 4
            encoded_buf.resize(4);
            encode_header(encoded_buf, message_buf.size());
            encoded_buf.insert(encoded_buf.end(),
                               std::make_move_iterator(message_buf.begin()),
                               std::make_move_iterator(message_buf.end())
                               );

            if(send_object->is_session()){
                auto session = send_object->get_session();
                session->write(encoded_buf);
                if(message_is_request){
                    session->read();
                }
            } else {
                //TODO: this is too session/boost specific
                //when using a test_socket or something like that this code
                //should be more abstract so that the message_handler does not need
                //to know what kind of object sends the data somewhere
                auto peer = send_object->get_peer();
                auto write_handler = std::make_shared<BoostSession>(io_context_, input_queue_);
                write_handler->write_to(encoded_buf, peer->get_ip(), peer->get_port());
                send_object->set_connection(write_handler);
                if(message_is_request){
                    write_handler->read();
                }
            }
            send_object->set_time_point();
        }
    }

    void encode_header(std::vector<boost::uint8_t>& buf, unsigned size) const
    {
        //header size is 4
        assert(buf.size() >= 4);
        buf[0] = static_cast<boost::uint8_t>((size >> 24) & 0xFF);
        buf[1] = static_cast<boost::uint8_t>((size >> 16) & 0xFF);
        buf[2] = static_cast<boost::uint8_t>((size >> 8) & 0xFF);
        buf[3] = static_cast<boost::uint8_t>(size & 0xFF);
    }

    /*
     * Calls a handler function depending on the msg content asynchronously
     * TODO: Should validate msg first
     */
    void dispatch(std::unique_ptr<RequestObject> data_object)
    {
        auto is_request = data_object->is_request();
        /*
         * We have to capture the data_obj by move. otherwise the session shared_ptr
         * will call destructor when this function runs out of scope.
         */
        if(is_request){
            //TODO: is this creating copy of msg_handler_? if so i have to pass std::ref() instead
            std::thread t([this, data_object = std::move(data_object)]() mutable
                { msg_handler_->handle_request(std::move(data_object)); });
            t.detach();
        } else {
            std::thread t([this, data_object = std::move(data_object)]() mutable
                { msg_handler_->handle_response(std::move(data_object)); });
            t.detach();
        }
    }

    void send_routing_information_internal()
    {
        if(not run_) {
            return;
        }
        auto routing_info = msg_handler_->get_routing_information();
        auto pre = std::get<0>(routing_info);
        auto self = std::get<1>(routing_info);
        auto succ = std::get<2>(routing_info);
        if(not self.is_valid()){
            return;
        }

        tcp::resolver resolver(io_context_);
        auto endpoint = resolver.resolve("127.0.0.1", "8080");
        tcp::socket socket(io_context_);
        boost::asio::connect(socket, endpoint);

        boost::asio::streambuf request;
        std::ostream request_stream(&request);

        using boost::property_tree::ptree;
        using boost::property_tree::read_json;
        using boost::property_tree::write_json;

        ptree root, info;
        info.put("successor", succ.get_id());
        info.put("predecessor", pre.get_id());
        root.put_child(self.get_id(), info);

        std::ostringstream buf;
        write_json (buf, root, false);
        std::string json = buf.str();

        request_stream << "POST / HTTP/1.1 \r\n";
        request_stream << "Host:" << "lol" << "\r\n";
        request_stream << "User-Agent: C/1.0";
        request_stream << "Content-Type: application/json; charset=utf-8 \r\n";
        request_stream << "Accept: */*\r\n";
        request_stream << "Content-Length: " << json.length() << "\r\n";
        request_stream << "Connection: close\r\n\r\n";  //NOTE THE Double line feed
        request_stream << json;

        // Send the request.
        boost::asio::write(socket, request);
        std::this_thread::sleep_for(std::chrono::seconds(5));
        send_routing_information_internal();
    }

private:
    boost::asio::io_context io_context_;
    std::shared_ptr<ConcurrentQueue<MsgBufPair>> input_queue_;
    std::shared_ptr<ConcurrentQueue<RequestObject>>  output_queue_;
    std::shared_ptr<MessageHandler> msg_handler_;

    std::vector<std::thread> thread_pool_deprecated_;
    std::vector<std::thread> asio_pool_;

    bool run_ = true;

};

} // closing namespace peerpaste
#endif /* ifndef MESSAGE_DISPATCHER */
