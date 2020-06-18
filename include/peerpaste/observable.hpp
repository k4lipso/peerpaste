#pragma once

#include <string>
#include <vector>

#include "peerpaste/request_object.hpp"

class ObserverBase;
class RequestObject;

class Observable
{
public:
	void Attach(ObserverBase *const observer);
	void Notify(const RequestObject &notification);
	void Notify(const RequestObject &request_object, HandlerObject<HandlerFunction> handler);
	void Notify();

protected:
	std::vector<ObserverBase *> observers_;
};
