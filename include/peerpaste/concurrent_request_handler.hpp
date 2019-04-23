#ifndef CONCURRENT_REQUEST_HANDLER
#define CONCURRENT_REQUEST_HANDLER

#include <map>
#include <mutex>

namespace peerpaste {

template<typename T>
class ConcurrentRequestHandler
{
public:
    ConcurrentRequestHandler();

    T& operator[](std::string id)
    {
    	return requests_[id];
    }

    void set(const std::string& id, T request)
    {
        std::scoped_lock lk(mutex_);
        requests_[id] = request;
    }

    T get(const std::string& id)
    {
        std::scoped_lock lk(mutex_);
        return requests_[id];
    }

    bool try_find_and_erase(const std::string& id, T& request)
    {
        std::scoped_lock lk(mutex_);
        auto search = request_.find(id);
        if(search == requests_.end()){
            return false;
        }
        request = requests_.at(id);
        requests_.erase(id);
        return true;
    }

    void handle_timeouts()
    {
        //for every obj in open_request
        for(auto iter = requests_.begin();
                 iter != requests_.end(); iter++){
            auto req_obj = iter->second;
            //if its still valid continue
            if(req_obj->is_valid()) continue;
            std::cout << req_obj->get_request_type() << std::endl;
            //call handler_function with original obj
            //that way it is still a request and will be catched by the handler
            //when checking if it is a response
            req_obj->call(req_obj);
            //erase object from open_request
            requests_.erase(iter);
        }
    }

private:
    std::map<std::string, T> requests_;
    mutable std::mutex mutex_;
};

} //closing namespace peerpaste

#endif /* ifndef CONCURRENT_REQUEST_HANDLER */
