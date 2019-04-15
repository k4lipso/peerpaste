#ifndef REQUEST_OBJECT_HPP
#define REQUEST_OBJECT_HPP

#include "message.hpp"
#include "peer.hpp"
#include "session.hpp"

#include <variant>
#include <optional>

class RequestObject;

using MessagePtr = std::shared_ptr<Message>;
using PeerPtr = std::shared_ptr<Peer>;
using SessionPtr = std::shared_ptr<Session>;
using RequestObjectUPtr = std::unique_ptr<RequestObject>;
using HandlerFunction = std::function<void(RequestObjectUPtr)>;

class RequestObject
{
public:

    RequestObject() {
        start_ = std::chrono::steady_clock::now();
    }
    ~RequestObject () {}

    /*
     * Calls the optional handlerfunction with the
     * given request object
     * The Idea is that you create a Request, set a handler
     * and store it in an open_requests container
     * As soon as you get a response with a matching
     * corelational ID you pass the response object
     * to this call function of the original request object
     * which will execute the function with the response as an
     * argument
     */
    void call(RequestObjectUPtr request)
    {
        //throw exeption if no value
        if(handler_.has_value()) handler_.value()(std::move(request));
    }

    void call(std::shared_ptr<RequestObject> request)
    {
        //throw exeption if no value
        auto unique_request = std::make_unique<RequestObject>(*request.get());
        if(handler_.has_value()) handler_.value()(std::move(unique_request));
    }

    void set_handler(HandlerFunction handler)
    {
        handler_ = handler;
    }

    const bool has_handler() const
    {
        return handler_.has_value();
    }

    const HandlerFunction get_handler() const
    {
        return handler_.value();
    }

    void set_message(MessagePtr message)
    {
        message_ = message;
    }

    const MessagePtr get_message() const
    {
        return message_;
    }

    void set_connection(PeerPtr peer)
    {
        connection_ = peer;
    }

    void set_connection(SessionPtr session)
    {
        connection_ = session;
    }

    const bool is_session() const
    {
        return std::holds_alternative<SessionPtr>(connection_);
    }

    const SessionPtr get_session() const
    {
        return std::get<SessionPtr>(connection_);
    }

    const PeerPtr get_peer() const
    {
        return std::get<PeerPtr>(connection_);
    }

    const std::string get_correlational_id() const noexcept
    {
        return message_->get_correlational_id();
    }

    const std::string get_request_type() const noexcept
    {
        return message_->get_request_type();
    }

    const std::string get_client_ip() const
    {
        if(is_session()){
            return get_session()->get_client_ip();
        }
        return get_peer()->get_ip();
    }

    void set_time_point(){
        start_ = std::chrono::steady_clock::now();
    }

    auto get_time_point(){
        return start_;
    }

    auto is_valid(){
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::seconds>(end - start_).count();
        std::cout << "dur = " << dur << std::endl;
        //at startup duration is somehow > 1000 so we filter by the && statement
        //TODO: find why dur is so bit at the beginning
        if(dur > 10){
            return false;
        }
        return true;
    }

private:
    MessagePtr message_;
    std::optional<HandlerFunction> handler_;
    std::variant<PeerPtr, SessionPtr> connection_;
    std::chrono::steady_clock::time_point start_;
};

#endif /* ifndef REQUEST_OBJECT_HPP */
