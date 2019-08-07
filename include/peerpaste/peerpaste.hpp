#ifndef PEERPASTE
#define PEERPASTE

#include <memory>
#include <string>
#include <thread>

#include "peerpaste/cryptowrapper.hpp"
#include "peerpaste/server.hpp"

namespace peerpaste {

    class PeerPaste
    {
        public:

            ~PeerPaste() {
                stop();
            }

            void init(unsigned port, size_t thread_count)
            {
                handler_ = std::make_shared<MessageHandler>(port);
                dispatcher_ = std::make_unique<peerpaste::MessageDispatcher>(handler_);
                server_ = std::make_unique<Server>(thread_count, port, dispatcher_->get_context());
                server_->set_queue(dispatcher_->get_receive_queue());
                handler_->init(dispatcher_->get_send_queue());
            }

            void run()
            {
                if(joined_ or create_ring_){
                    handler_->run();
                }
                //TODO: server has to run even if for put/get. that means that on a put/get the
                //      responing node is somehow creating a new tcp session which gets not accepted
                //      when server is not listening. that is bad because even for simple put/get
                //      a port forwarding for hosts behind nat would be needed. this must change
                server_->run();
                dispatcher_->run();
            }

            void stop()
            {
                server_->stop();
                handler_->stop();
                dispatcher_->stop();
            }

            void join(const std::string& ip, const std::string& port)
            {
                handler_->join(ip, port);
                joined_ = true;
            }

            void create_ring(){
                create_ring_ = true;
            }

            void debug()
            {
                dispatcher_->send_routing_information();
            }

            std::future<std::string> async_put(const std::string& ip,
                                               const std::string& port,
                                               const std::string& filename)
            {
                std::ifstream f(filename);
                std::string str((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());
                return handler_->put(ip, port, str);
            }

            std::future<std::string> async_get(const std::string& ip,
                                               const std::string& port,
                                               const std::string& data)
            {
                return handler_->get(ip, port, data);
            }

            void wait_till_finish(){
                dispatcher_->join();
            }

        private:
            bool joined_ = false;
            bool create_ring_ = false;
            std::unique_ptr<Server> server_ = nullptr;
            std::unique_ptr<peerpaste::MessageDispatcher> dispatcher_ = nullptr;
            std::shared_ptr<MessageHandler> handler_ = nullptr;
    };

}

#endif /* ifndef PEERPASTE */
