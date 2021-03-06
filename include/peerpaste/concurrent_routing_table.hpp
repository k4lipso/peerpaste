#ifndef CONCURRENT_ROUTING_TABLE
#define CONCURRENT_ROUTING_TABLE

#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include "peerpaste/cryptowrapper.hpp"

namespace peerpaste
{

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
	std::vector<T> successor_list_;
	static constexpr int succ_list_size_ = 10;

public:
	ConcurrentRoutingTable()
	{
	}

	bool try_get_self(T &value) const
	{
		std::scoped_lock lk(mutex_);
		if(self_ == nullptr)
		{
			self_is_set = false;
			return false;
		}
		value = *self_.get();
		return true;
	}

	bool has_self() const
	{
		std::scoped_lock lk(mutex_);
		return self_ != nullptr;
	}

	void set_self(T self)
	{
		std::scoped_lock lk(mutex_);
		self_ = std::make_unique<T>(std::move(self));
		self_is_set = true;
	}

	bool try_get_predecessor(T &value) const
	{
		std::scoped_lock lk(mutex_);
		if(predecessor_ == nullptr)
		{
			predecessor_is_set = false;
			return false;
		}
		value = *predecessor_.get();
		return true;
	}

	bool has_predecessor() const
	{
		std::scoped_lock lk(mutex_);
		if(predecessor_ == nullptr)
			return false;
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

	bool try_get_successor(T &value) const
	{
		std::scoped_lock lk(mutex_);
		if(successor_list_.empty())
		{
			successor_is_set = false;
			return false;
		}
		value = successor_list_.front();
		return true;
	}

	bool has_sucessor() const
	{
		std::scoped_lock lk(mutex_);
		if(successor_list_.empty())
			return false;
		return true;
	}

	void set_successor(T sucessor)
	{
		std::scoped_lock lk(mutex_);
		if(successor_list_.empty())
		{
			successor_list_.push_back(std::move(sucessor));
		}
		else
		{
			successor_list_.at(0) = std::move(sucessor);
		}
		successor_is_set = true;
		condition_.notify_all();
	}

	void replace_successor_list(std::vector<T> new_succ_list)
	{
		std::scoped_lock lk(mutex_);
		successor_list_ = std::move(new_succ_list);
		successor_list_.resize(succ_list_size_);
	}

	void replace_after_successor(std::vector<T> new_succ_list)
	{
		std::scoped_lock lk(mutex_);
		successor_list_.insert(successor_list_.begin() + 1, new_succ_list.begin(), new_succ_list.end());
		successor_list_.resize(succ_list_size_);
	}

	size_t size() const
	{
		std::scoped_lock lk(mutex_);
		return successor_list_.size();
	}

	void push_back(T value)
	{
		std::scoped_lock lk(mutex_);
		successor_list_.push_back(std::move(value));
	}

	std::vector<T> get_peers() const
	{
		std::scoped_lock lk(mutex_);
		return successor_list_;
	}

	bool pop_front()
	{
		std::scoped_lock lk(mutex_);
		if(successor_list_.size() <= 1)
		{
			spdlog::warn("Cant pop fron, succ list to small");
			return false;
		}

		successor_list_.erase(successor_list_.begin());
		return true;
	}

	void wait_til_valid()
	{
		std::unique_lock lk(mutex_);
		condition_.wait(lk, [this] { return self_is_set && predecessor_is_set && successor_is_set; });
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
		if(predecessor_ != nullptr)
		{
			predecessor_->print();
		}
		std::cout << "FINGERTABLE: " << '\n';
		for(const auto &element : successor_list_)
		{
			element.print();
		}
		/* for (auto it = successor_list_.begin(); */
		/*      it < successor_list_.end(); it++) { */
		/*     if(*it != nullptr){ */
		/*         *it->print(); */
		/*     } */
		/*     std::cout << "######" << '\n'; */
		/* } */
		std::cout << "#### ROUTINGTABLE END ####" << '\n';
	}
};
} // namespace peerpaste
#endif /* ifndef CONCURRENT_ROUTING_TABLE */
