#pragma once

#include <future>
#include <atomic>
#include <vector>
#include <string>
#include <optional>

#include "peerpaste/observable.hpp"
#include "peerpaste/observer_base.hpp"

class DummyMessage;
class MessagingBase : public Observable, public ObserverBase
{
public:
  using Observable::Observable;

  MessagingBase(MessageType type);
  explicit MessagingBase(MessagingBase&& other);
  virtual ~MessagingBase();

  virtual void operator()() = 0;

  bool operator<(const MessagingBase& other) const;

  bool is_done() const noexcept;
  bool is_waiting() const;
  bool awaits(const std::string& id) const;
  std::string awaits() const;
  std::future<std::string> get_future();

protected:
  //Only root object needs promise. leafobjects do not.
  std::promise<std::string> promise_;
  std::optional<std::string> correlational_id_;
  std::vector<std::unique_ptr<MessagingBase>> dependencies_;
  std::atomic<bool> is_done_ = false;
  MessageType type_;
};
