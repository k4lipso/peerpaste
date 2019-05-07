#include "peerpaste/aggregat.hpp"

Aggregat::Aggregat(RequestObjectPtr request) : request_(request) {}
Aggregat::Aggregat(RequestObjectPtr request, std::unordered_set<std::string> ids)
                : request_(request),
                  correlational_ids(ids) {}

void Aggregat::add_id(const std::string& id)
{
    correlational_ids.insert(id);
}

const bool Aggregat::has_id(const std::string& id) const noexcept
{
    auto search = correlational_ids.find(id);
    if(search != correlational_ids.end()) {
        return true;
    }

    return false;
}

const bool Aggregat::is_complete() const noexcept
{
    return correlational_ids.empty();
}

//Returns true if message was added, false if not
bool Aggregat::add_message(MessagePtr message)
{
    if(!has_id(message->get_correlational_id())){
        /* std::cout << "id not found in add message" << std::endl; */
        return false;
    }

    if(message->is_request()){
        util::log(warning, "add_message failed, message is no response");
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

const RequestObjectPtr Aggregat::get_result_message() const
{
    auto original_message = request_->get_message();

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
    if(original_message->get_request_type() == "put"){
        //put request aggregat is waiting for a data id which is sent
        //when the data was stored successfully
        //TODO: validation
        auto data_id = messages_.front()->get_data();
        original_message->set_data(data_id);
        /* original_message->generate_transaction_id(); */
        return request_;
    }
    if(original_message->get_request_type() == "get"){
        auto data = messages_.front()->get_data();
        original_message->set_data(data);
        return request_;
    }
    if(original_message->get_request_type() == "put_dummy"){
        auto hash_encrypted = messages_.front()->get_data();
        auto hash_unencrypted = original_message->get_data();
        hash_unencrypted += hash_encrypted;
        util::log(notify, hash_unencrypted);
        return nullptr;
    }
    if(original_message->get_request_type() == "get_dummy"){
        auto message_encrypted = messages_.front()->get_data();
        auto data_key = original_message->get_data();
        util::log(notify, util::decrypt(data_key, message_encrypted));
        return nullptr;
    }
    /* if(original_message->get_request_type() == "get"){ */
    /*     auto succ = messages_.front()->get_peers().at(0); */
    /*     request_->set_connection(std::make_shared<Peer>(succ)); */
    /*     std::cout << "return get" << std::endl; */
    /*     return request_; */
    /* } */

    std::cout << "No pattern matched in get_result_message()" << '\n';
    return nullptr;
}
