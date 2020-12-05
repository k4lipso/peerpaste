#include <algorithm>

#include "peerpaste/observable.hpp"
#include "peerpaste/observer_base.hpp"

void Observable::Attach(std::weak_ptr<ObserverBase> observer)
{
	observers_.push_back(observer);
}

void Observable::Notify(const RequestObject &notification)
{
	std::for_each(std::begin(observers_), std::end(observers_), [&notification](auto& o) {
		if(auto ptr = o.lock())
		{
			ptr->HandleNotification(notification);
		}
		else
		{
			std::cout << "1Tried to notify dead observer onnhhnoooo\n";
		}
	});
}

void Observable::Notify(const RequestObject &notification, HandlerObject<HandlerFunction> handler)
{
	std::for_each(std::begin(observers_), std::end(observers_), [&notification, &handler](auto& o) {
		if(auto ptr = o.lock())
		{
			ptr->HandleNotification(notification, handler);
		}
		else
		{
			std::cout << "2Tried to notify dead observer onnhhnoooo\n";
		}
	});
}

void Observable::Notify()
{
	std::for_each(std::begin(observers_), std::end(observers_), [](auto& o)
	{
		if(auto ptr = o.lock())
		{
			ptr->HandleNotification();
		}
		else
		{
			std::cout << "3Tried to notify dead observer onnhhnoooo\n";
		}
	});
}
