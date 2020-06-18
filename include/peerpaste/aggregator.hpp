#ifndef AGGREGATOR_HPP
#define AGGREGATOR_HPP

#include <mutex>

#include "aggregat.hpp"
#include "message.hpp"
#include "request_object.hpp"

using MessagePtr = std::shared_ptr<Message>;
using RequestObjectPtr = std::shared_ptr<RequestObject>;

class Aggregator
{
	mutable std::mutex mutex_;
	std::vector<Aggregat> aggregats_;

public:
	Aggregator();
	const RequestObjectPtr add_aggregat(MessagePtr message);
	void add_aggregat(RequestObjectPtr request, std::unordered_set<std::string> ids);
};

#endif /* ifndef AGGREGATOR_HPP */
