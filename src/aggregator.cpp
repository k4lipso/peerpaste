#include "peerpaste/aggregator.hpp"

Aggregator::Aggregator(){}

const RequestObjectPtr Aggregator::add_aggregat(MessagePtr message)
{
    //for every aggregat
    for(auto aggregat = aggregats_.begin();
             aggregat != aggregats_.end();
             aggregat++){

        //skip if the message doesnt match the aggregat
        if(not aggregat->add_message(message)){
            continue;
        }

        //if complete return result message and erase aggregat
        if(aggregat->is_complete()){
            auto result = aggregat->get_result_message();
            aggregats_.erase(aggregat);
            return result;
        }
        break;
    }
    return nullptr;
}

void Aggregator::add_aggregat(RequestObjectPtr request, std::unordered_set<std::string> ids)
{
    aggregats_.push_back(Aggregat(request, ids));
}
