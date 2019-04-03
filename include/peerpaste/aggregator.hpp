#ifndef AGGREGATOR_HPP
#define AGGREGATOR_HPP

#include "message.hpp"
#include "aggregat.hpp"
#include "request_object.hpp"

using MessagePtr = std::shared_ptr<Message>;
using RequestObjectPtr = std::shared_ptr<RequestObject>;

class Aggregator
{
public:
    Aggregator();
    const RequestObjectPtr add_aggregat(MessagePtr message);
    void add_aggregat(RequestObjectPtr request, std::unordered_set<std::string> ids);

private:
    std::vector<Aggregat> aggregats_;
};

#endif /* ifndef AGGREGATOR_HPP */
