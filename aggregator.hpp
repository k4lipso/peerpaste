#ifndef AGGREGATOR_HPP
#define AGGREGATOR_HPP

#include "write_queue.hpp"
#include "message.hpp"
#include "aggregat.hpp"
#include "request_object.hpp"

class Aggregator
{
public:
    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<RequestObject> RequestObjectPtr;

    Aggregator () : aggregats_() {
        write_queue_ = WriteQueue::GetInstance();
    }

    const RequestObjectPtr add_aggregat(MessagePtr message)
    {
        auto id = message->get_correlational_id();
        for(auto aggregat : aggregats_){
            if(aggregat.add_message(message)){
                if(aggregat.is_complete()){
                    //TODO: delete aggregat from vector
                    return aggregat.get_result_message();
                }
                return nullptr;
            }
        }
        std::cout << "AGGR NOT IN AGGRS" << std::endl;
        return nullptr;
    }

    void add_aggregat(RequestObjectPtr request, std::unordered_set<std::string> ids)
    {
        aggregats_.push_back(Aggregat(request, ids));
    }

private:
    std::shared_ptr<WriteQueue> write_queue_;
    std::vector<Aggregat> aggregats_;
};

#endif /* ifndef AGGREGATOR_HPP */
