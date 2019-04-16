#include <mutex>
#include <vector>
#include <memory>
#include <condition_variable>

namespace peerpaste {

template<typename T>
class ConcurrentRoutingTable
{
    mutable std::mutex mutex_;
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
            return false;
        }
        value = *self_.get();
    }

    void set_self(T self)
    {
        std::scoped_lock lk(mutex_);
        self_ = std::make_unique<T>(std::move(self));
    }

    bool try_get_predecessor(T& value) const
    {
        std::scoped_lock lk(mutex_);
        if(predecessor_ == nullptr){
            return false;
        }
        value = *predecessor_.get();
    }

    void set_predecessor(T predecessor)
    {
        std::scoped_lock lk(mutex_);
        predecessor_ = std::make_unique<T>(std::move(predecessor));
        condition_.notify_all();
    }

    bool try_get_successor(T& value) const
    {
        std::scoped_lock lk(mutex_);
        if(peers_.front() == nullptr){
            return false;
        }
        value = *peers_.front().get();
    }

    void set_successor(T sucessor)
    {
        std::scoped_lock lk(mutex_);
        peers_.at(0) = std::make_unique<T>(std::move(sucessor));
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

    std::vector<std::unique_ptr<T>> get_peers() const
    {
        std::scoped_lock lk(mutex_);
        return peers_;
    }

    void wait_til_valid()
    {
        std::unique_lock lk(mutex_);
        condition_.wait(lk, [this]{ return is_valid(); });
    }

    /*
     * Checks if successor and predecessor are set
     */
    bool is_valid()
    {
        std::scoped_lock lk(mutex_);

        if(not (peers_.at(0) != nullptr && peers_.at(0) != self_)){
            return false;
        }
        if(predecessor_ != nullptr && predecessor_ != self_){
            return true;
        }
        return false;
    }

    void print() const
    {
        return;
        /* std::cout << "#### ROUTINGTABLE BEGIN ####" << '\n'; */
        /* std::cout << "SELF:" << '\n'; */
        /* self_->print(); */
        /* std::cout << "PREDECESSOR: " << '\n'; */
        /* if(predecessor_ != nullptr){ */
        /*     predecessor_->print(); */
        /* } */
        /* std::cout << "FINGERTABLE: " << '\n'; */
        /* for(const auto peer : peers_){ */
        /*     if(peer != nullptr){ */
        /*         peer->print(); */
        /*     } */
        /*     std::cout << "######" << '\n'; */
        /* } */
        /* std::cout << "#### ROUTINGTABLE END ####" << '\n'; */
    }

};
} // closing namespace peerpaste
