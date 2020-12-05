#pragma once

#include <string>
#include <vector>
#include <memory>

#include "peerpaste/request_object.hpp"

class ObserverBase;
class RequestObject;

class Observable
{
public:
	void Attach(std::weak_ptr<ObserverBase> observer);
	void Notify(const RequestObject &notification);
	void Notify(const RequestObject &request_object, HandlerObject<HandlerFunction> handler);
	void Notify();

protected:
	std::vector<std::weak_ptr<ObserverBase>> observers_;
};
