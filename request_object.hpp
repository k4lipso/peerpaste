#ifndef REQUEST_OBJECT_HPP
#define REQUEST_OBJECT_HPP

#include "message.hpp"
#include "peer.hpp"
#include "session.hpp"

#include <variant>
#include <optional>


class RequestObject
{
public:
    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<Peer> PeerPtr;
    typedef std::shared_ptr<Session> SessionPtr;
    typedef std::function<void(MessagePtr)> HandlerFunction;

    RequestObject() {}
    ~RequestObject () {}

    void call(MessagePtr message)
    {
        //throw exeption if no value
        if(handler_.has_value()) handler_.value()(message);
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

private:
    std::optional<HandlerFunction> handler_;
    MessagePtr message_;
    std::variant<PeerPtr, SessionPtr> connection_;
};

#endif /* ifndef REQUEST_OBJECT_HPP */
