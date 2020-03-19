#pragma once

#include <future>
#include <atomic>
#include <vector>
#include <string>
#include <optional>

#include "peerpaste/observable.hpp"
#include "peerpaste/observer_base.hpp"
#include "peerpaste/request_object.hpp"

using HandlerFunction = std::function<void(RequestObject)>;

class MessagingBase : public Observable, public ObserverBase, public std::enable_shared_from_this<MessagingBase>
{
public:
  MessagingBase(MessageType type);
  MessagingBase(MessageType type, RequestObject request);
  explicit MessagingBase(MessagingBase&& other);
  virtual ~MessagingBase();

  virtual void operator()();

  bool is_done() const noexcept;
  bool is_request() const noexcept;
  bool IsType(const MessageType& type) const noexcept;
  MessageType GetType() const noexcept;

  std::future<std::string> get_future();
  std::chrono::time_point<std::chrono::system_clock> get_timeout() const;

protected:
  virtual void create_handler_object(const std::string& correlation_id, HandlerFunction handler_function);

  virtual void create_request() = 0;
  virtual void handle_request() = 0;
  virtual void handle_response(RequestObject request_object) = 0;

  //Only root object needs promise. leafobjects do not.
  MessageType type_;
  std::promise<std::string> promise_;
  std::unique_ptr<HandlerObject<HandlerFunction>> handler_object_ = nullptr;
  std::optional<RequestObject> request_;
  std::vector<std::unique_ptr<MessagingBase>> dependencies_;
  std::chrono::time_point<std::chrono::system_clock> time_point_;
  std::atomic<bool> is_done_ = false;
  std::atomic<bool> is_request_handler_ = false;
  static constexpr std::chrono::milliseconds DURATION{10000};
};
