#include <optional>

#include "peerpaste/message.hpp"
#include "peerpaste/peer.hpp"
#include "peerpaste/boost_session.hpp"
#include "peerpaste/request_object.hpp"

#include "peerpaste/concurrent_queue.hpp"
#include "peerpaste/messaging_base.hpp"
#include "peerpaste/thread_pool.hpp"
#include "peerpaste/concurrent_routing_table.hpp"
#include "peerpaste/peer.hpp"


namespace peerpaste { namespace message
{

class Notification : public MessagingBase
{
public:
  using MessagingBase::MessagingBase;
  Notification(ConcurrentRoutingTable<Peer>* routing_table);
  Notification(ConcurrentRoutingTable<Peer>* routing_table, RequestObject request);
  explicit Notification(Notification&& other);

  ~Notification() override;

  virtual void operator()() override;
  virtual void HandleNotification(const RequestObject& request_object) override;

private:
  void create_request();
  void handle_request();

  ConcurrentRoutingTable<Peer>* routing_table_;
  std::optional<RequestObject> request_;
  bool is_request_handler_;

};

} //closing namespce message
} //closing namespace peerpaste
