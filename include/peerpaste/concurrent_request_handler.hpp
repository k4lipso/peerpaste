#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <set>

#include "peerpaste/cryptowrapper.hpp"

namespace peerpaste
{

template<typename T, typename Compare = std::less<T>>
class ConcurrentSet
{
public:
	template<typename KeyType>
	bool contains(const KeyType &key) const
	{
		std::scoped_lock lk{mutex_};
		return set_.contains(key);
	}

	template<typename KeyType>
	std::optional<T> get_and_erase(const KeyType &key)
	{
		std::scoped_lock lk{mutex_};

		const auto search = set_.find(key);
		if(set_.find(key) == set_.end())
		{
			return {};
		}

		std::optional<T> return_value{std::move(*search)};
		set_.erase(search);
		return return_value;
	}

	template<typename KeyType, typename Pred>
	std::optional<T> get_and_erase_if(const KeyType &key, const Pred& pred)
	{
		std::scoped_lock lk{mutex_};

		const auto search = set_.find(key);
		if(set_.find(key) == set_.end())
		{
			return {};
		}

		if(pred(*search))
		{
			std::optional<T> return_value{std::move(*search)};
			set_.erase(search);
			return return_value;
		}

		return *search;
	}

	void insert(const T &value)
	{
		std::scoped_lock lk{mutex_};
		set_.insert(value);
	}

	void emplace(T &&value)
	{
		std::scoped_lock lk{mutex_};
		set_.emplace(std::forward<T>(value));
	}

	template<typename Pred>
	void erase_if(Pred predicate)
	{
		std::scoped_lock lk{mutex_};
		for(auto it = set_.begin(); it != set_.end();)
		{
			if(predicate(*it))
			{
				it = set_.erase(it);
			}
			else
			{
				++it;
			}
		}

		// std::erase_if(set_, predicate);
	}

private:
	mutable std::mutex mutex_;
	std::set<T, Compare> set_;
};

namespace deprecated
{
template<typename T>
class ConcurrentRequestHandler
{
public:
	ConcurrentRequestHandler()
	{
	}

	T &operator[](const std::string &id)
	{
		std::scoped_lock lk(mutex_);
		return requests_[id];
	}

	void set(const std::string &id, T request)
	{
		std::scoped_lock lk(mutex_);
		requests_[id] = request;
	}

	T get(const std::string &id)
	{
		std::scoped_lock lk(mutex_);
		return requests_[id];
	}

	bool try_find_and_erase(const std::string &id, T &request)
	{
		std::scoped_lock lk(mutex_);
		auto search = requests_.find(id);
		if(search == requests_.end())
		{
			return false;
		}
		request = requests_.at(id);
		requests_.erase(id);
		return true;
	}

	void handle_timeouts()
	{
		spdlog::debug("RequestHandler::handle_timeouts()");
		std::unique_lock lk(mutex_);
		for(auto iter = requests_.begin(); iter != requests_.end();)
		{
			auto req_obj = iter->second;
			// if its still valid continue
			if(req_obj->is_valid())
			{
				++iter;
				continue;
			}
			// erase object from open_request
			iter = requests_.erase(iter);
			// call handler_function with original obj
			// that way it is still a request and will be catched by the handler
			// when checking if it is a response
			lk.unlock();
			req_obj->call(req_obj);
			break;
		}
	}

private:
	std::map<std::string, T> requests_;
	mutable std::mutex mutex_;
};

} // namespace deprecated
} // namespace peerpaste
