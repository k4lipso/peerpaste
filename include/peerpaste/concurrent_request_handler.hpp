#pragma once

#include <map>
#include <set>
#include <mutex>
#include <optional>

#include "peerpaste/cryptowrapper.hpp"

namespace peerpaste
{


template<typename T, typename KeyType = T>
class ConcurrentRequestHandler
{
public:
  bool contains(const KeyType& key) const
  {
    std::scoped_lock lk{mutex_};
    return set_.contains(key);
  }

  std::optional<T> get_and_erase(const KeyType& key)
  {
    std::scoped_lock lk{mutex_};

    auto search = set_.find(key);
    if(search != set_.end())
    {
      std::optional<T> return_value{*search};
      set_.erase(search);
      return return_value;
    }
    else
    {
      return {};
    }
  }

  void insert(const T& value)
  {
    std::scoped_lock lk{mutex_};
    set_.insert(value);
  }

private:
  mutable std::mutex mutex_;
  std::set<T> set_;
};

namespace deprecated {
template<typename T>
class ConcurrentRequestHandler
{
public:
    ConcurrentRequestHandler() {}

    T& operator[](const std::string& id)
    {
        std::scoped_lock lk(mutex_);
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
        auto search = requests_.find(id);
        if(search == requests_.end()){
            return false;
        }
        request = requests_.at(id);
        requests_.erase(id);
        return true;
    }

    void handle_timeouts()
    {
        util::log(debug, "RequestHandler::handle_timeouts()");
        std::unique_lock lk(mutex_);
        for(auto iter = requests_.begin();
                 iter != requests_.end();){
            auto req_obj = iter->second;
            //if its still valid continue
            if(req_obj->is_valid()) {
                ++iter;
                continue;
            }
            //erase object from open_request
            iter = requests_.erase(iter);
            //call handler_function with original obj
            //that way it is still a request and will be catched by the handler
            //when checking if it is a response
            lk.unlock();
            req_obj->call(req_obj);
            break;
        }
    }

private:
    std::map<std::string, T> requests_;
    mutable std::mutex mutex_;
};

} // closing namespace deprecated
} //closing namespace peerpaste
