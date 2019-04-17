#ifndef CONCURRENT_ROUTING_TABLE
#define CONCURRENT_ROUTING_TABLE

#include <mutex>
#include <vector>
#include <memory>
#include <condition_variable>

namespace peerpaste {

template<typename T>
class ConcurrentRoutingTable
{
    mutable std::mutex mutex_;
    mutable bool self_is_set;
    mutable bool predecessor_is_set;
    mutable bool successor_is_set;
    std::condition_variable condition_;

    std::unique_ptr<T> self_;
    std::unique_ptr<T> predecessor_;
    std::vector<std::unique_ptr<T>> peers_;

public:

    ConcurrentRoutingTable()
    {
        peers_.push_back(nullptr);
    }

    bool try_get_self(T& value) const
    {
        std::scoped_lock lk(mutex_);
        if(self_ == nullptr){
            self_is_set = false;
            return false;
        }
        value = *self_.get();
        return true;
    }

    bool has_self() const
    {
        std::scoped_lock lk(mutex_);
        if(self_ == nullptr) return false;
        return true;
    }

    void set_self(T self)
    {
        std::scoped_lock lk(mutex_);
        self_ = std::make_unique<T>(std::move(self));
        self_is_set = true;
    }

    bool try_get_predecessor(T& value) const
    {
        std::scoped_lock lk(mutex_);
        if(predecessor_ == nullptr){
            predecessor_is_set = false;
            return false;
        }
        value = *predecessor_.get();
        return true;
    }

    bool has_predecessor() const
    {
        std::scoped_lock lk(mutex_);
        if(predecessor_ == nullptr) return false;
        return true;
    }

    void set_predecessor(T predecessor)
    {
        std::scoped_lock lk(mutex_);
        predecessor_ = std::make_unique<T>(std::move(predecessor));
        predecessor_is_set = true;
        condition_.notify_all();
    }

    void reset_predecessor()
    {
        std::scoped_lock lk(mutex_);
        predecessor_ = nullptr;
    }

    bool try_get_successor(T& value) const
    {
        std::scoped_lock lk(mutex_);
        if(peers_.front() == nullptr){
            successor_is_set = false;
            return false;
        }
        value = *peers_.front().get();
        return true;
    }

    bool has_sucessor() const
    {
        std::scoped_lock lk(mutex_);
        if(peers_.front() == nullptr) return false;
        return true;
    }

    void set_successor(T sucessor)
    {
        std::scoped_lock lk(mutex_);
        peers_.at(0) = std::make_unique<T>(std::move(sucessor));
        successor_is_set = true;
        condition_.notify_all();
    }

    size_t size() const
    {
        std::scoped_lock lk(mutex_);
        return peers_.size();
    }

    void push_back(T value)
    {
        std::scoped_lock lk(mutex_);
        peers_.push_back(std::make_unique<T>(value));
    }

    /*
     * Copyconstruct a vec<T> from the vec<unique_ptr<T>>
     * TODO: use std::transform
     */
    std::vector<T> get_peers() const
    {
        std::vector<T> result;

        std::scoped_lock lk(mutex_);
        for (auto it = peers_.begin();
             it < peers_.end(); it++) {
            if(*it == nullptr) break;
            result.push_back(*it->get());
        }

        return result;
    }

/*        std::vector<int> i { 1, 2, 3, 4}; */
/*     std::vector<std::string> str_vec; */
/*     std::transform(i.begin(), i.end(), std::back_inserter(str_vec), */
/*         [](int j) -> std::string { return std::to_string(j); }); */

    void wait_til_valid()
    {
        std::unique_lock lk(mutex_);
        condition_.wait(lk, [this]{
                    return self_is_set &&
                           predecessor_is_set &&
                           successor_is_set;
                    });
    }

    /*
     * Checks if successor and predecessor are set
     */
    bool is_valid()
    {
        std::scoped_lock lk(mutex_);
        return self_is_set && predecessor_is_set && successor_is_set;
    }

    void print() const
    {
        std::scoped_lock lk(mutex_);
        std::cout << "#### ROUTINGTABLE BEGIN ####" << '\n';
        std::cout << "SELF:" << '\n';
        self_->print();
        std::cout << "PREDECESSOR: " << '\n';
        if(predecessor_ != nullptr){
            predecessor_->print();
        }
        std::cout << "FINGERTABLE: " << '\n';
        if(peers_.at(0) != nullptr){
            peers_.at(0)->print();
        }
        /* for (auto it = peers_.begin(); */
        /*      it < peers_.end(); it++) { */
        /*     if(*it != nullptr){ */
        /*         *it->print(); */
        /*     } */
        /*     std::cout << "######" << '\n'; */
        /* } */
        std::cout << "#### ROUTINGTABLE END ####" << '\n';
    }

};
} // closing namespace peerpaste
#endif /* ifndef CONCURRENT_ROUTING_TABLE */
