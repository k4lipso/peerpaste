#include <algorithm>

#include "peerpaste/observable.hpp"
#include "peerpaste/observer_base.hpp"

void Observable::Attach(ObserverBase* const observer) { observers_.push_back(observer); }

void Observable::Notify(const RequestObject& notification)
{
    std::for_each(std::begin(observers_), std::end(observers_),
                  [&notification](ObserverBase *o) { o->HandleNotification(notification); });
}
