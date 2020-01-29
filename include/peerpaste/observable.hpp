#pragma once

#include <vector>
#include <string>

#include "peerpaste/request_object.hpp"

class ObserverBase;
class RequestObject;

class Observable {
public:
  void Attach(ObserverBase* const observer);
  void Notify(const RequestObject& notification);

protected:
  std::vector<ObserverBase *> observers_;
};
