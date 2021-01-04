#pragma once

#include <functional>
#include <string>

#include "peerpaste/request_object.hpp"

using HandlerFunction = std::function<void(RequestObject)>;

class ObserverBase
{
public:
	virtual ~ObserverBase();
	virtual void HandleNotification(const RequestObject &request_object) = 0;
	virtual void HandleNotification(const RequestObject&, HandlerObject<HandlerFunction>)
	{
	}
	virtual void HandleNotification()
	{
	}
};
