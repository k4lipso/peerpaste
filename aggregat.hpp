#ifndef AGGREGAT_HPP
#define AGGREGAT_HPP

#include "message.hpp"
#include "request_object.hpp"

#include <vector>
#include <set>
#include <memory>

class Aggregat
{
public:
    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<RequestObject> RequestObjectPtr;

    Aggregat(RequestObjectPtr request) : request_(request) {}
    Aggregat(RequestObjectPtr request, std::unordered_set<std::string> ids)
                    : request_(request),
                      correlational_ids(ids) {}

    void add_id(const std::string& id)
    {
        correlational_ids.insert(id);
    }

    const bool has_id(const std::string& id) const noexcept
    {
        auto search = correlational_ids.find(id);
        if(search != correlational_ids.end()) {
            return true;
        }

        return false;
    }

    const bool is_complete() const noexcept
    {
        return correlational_ids.empty();
    }

    //Returns true if message was added, false if not
    bool add_message(MessagePtr message)
    {
        if(!has_id(message->get_correlational_id())){
            std::cout << "id not found in add message" << std::endl;
            return false;
        }

        if(message->is_request()){
            std::cout << "add_message failed, message is no response"
                      << std::endl;
            return false;
        }

        auto id = message->get_correlational_id();
        auto pos = correlational_ids.find(message->get_correlational_id());
        //delete id from set
        correlational_ids.erase(pos);
        //add msg to vec
        messages_.push_back(message);
        return true;
    }

    const RequestObjectPtr get_result_message() const
    {
        auto original_message = request_->get_message();
        auto result = std::make_shared<RequestObject>();

        //If request is find_successor and in messages_ is only one object
        //with a query response, then this is a is a join
        if(original_message->get_request_type() == "find_successor"){
            if(messages_.size() == 1 && messages_.front()->get_request_type() == "query"){
                auto query_message = messages_.front();
                auto self = query_message->get_peers().front();
                original_message->set_peers({ self });
                original_message->generate_transaction_id();
                return request_;
            }
            //both messages are "find_successor"
            //so we forward the response from the second to the first
            if(messages_.size() == 1 && messages_.front()->get_request_type() == "find_successor"){
                auto successor = messages_.front()->get_peers().front();
                request_->get_message()->set_peers( { successor } );
                return request_;
            }
        }

        std::cout << "No pattern matched in get_result_message()" << '\n';
        return nullptr;
    }

private:
    std::unordered_set<std::string> correlational_ids;
    std::vector<MessagePtr> messages_;
    RequestObjectPtr request_;
};

#endif /* ifndef AGGREGAT_HPP */
