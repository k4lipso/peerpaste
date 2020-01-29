#pragma once

#include <string>

#include "peerpaste/request_object.hpp"

class RequestObject;

class ObserverBase {
public:
  virtual ~ObserverBase();
  virtual void HandleNotification(const RequestObject& request_object) = 0;
};
