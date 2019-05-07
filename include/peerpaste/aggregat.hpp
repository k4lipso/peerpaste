#ifndef AGGREGAT_HPP
#define AGGREGAT_HPP

#include "peerpaste/message.hpp"
#include "peerpaste/request_object.hpp"
#include "peerpaste/cryptowrapper.hpp"

#include <vector>
#include <set>
#include <memory>

using MessagePtr = std::shared_ptr<Message>;
using RequestObjectPtr = std::shared_ptr<RequestObject>;

class Aggregat
{
public:

    Aggregat(RequestObjectPtr request);
    Aggregat(RequestObjectPtr request, std::unordered_set<std::string> ids);

    void add_id(const std::string& id);
    const bool has_id(const std::string& id) const noexcept;
    const bool is_complete() const noexcept;
    //Returns true if message was added, false if not
    bool add_message(MessagePtr message);
    const RequestObjectPtr get_result_message() const;

private:
    std::unordered_set<std::string> correlational_ids;
    std::vector<MessagePtr> messages_;
    RequestObjectPtr request_;
};

#endif /* ifndef AGGREGAT_HPP */
