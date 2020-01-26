#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <fstream>
#include <thread>
#include <future>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/array.hpp>
#include <boost/program_options.hpp>

#include "peerpaste/cryptowrapper.hpp"
#include "peerpaste/server.hpp"
#include "peerpaste/peerpaste.hpp"
#include "peerpaste/thread_pool.hpp"
#include "peerpaste/messaging_base.hpp"

namespace po = boost::program_options;

class DummyMessage : public MessagingBase
{
public:
  DummyMessage(ThreadPool* const worker, unsigned dependency_count = 0) : worker_(worker)
  {
    for(unsigned i = 0; i < dependency_count; ++i)
    {
      dependencies_.emplace_back(std::make_unique<DummyMessage>(worker_));
    }
  }

	explicit DummyMessage(DummyMessage&& other)
		: MessagingBase(std::move(other))
		,	worker_(other.worker_)
	{}

	~DummyMessage()
	{}


  virtual void operator()() override
  {
    Notify("Starting...");

    if(dependencies_.empty())
    {
      is_done_ = true;
      Notify("Done...");
      promise_.set_value("no deps. closing");
      return;
    }

    for(auto& dep : dependencies_)
    {
      dep->Attach(this);
      worker_->submit([&dep]() { (*dep)(); });
    }
  }

  virtual void HandleNotification(const std::string& msg) override
  {
    std::cout << "DummyMessage got notified: " << msg << std::endl;

    if(std::all_of(dependencies_.begin(), dependencies_.end(), [](const auto& dep) { return dep->is_done(); }))
    {
      is_done_ = true;
      Notify("Finished");
      promise_.set_value("promisevlaue: FIN");
    }
  }

private:
  ThreadPool* const worker_;
};

class Observer : public ObserverBase
{
public:
  virtual void HandleNotification(const std::string& msg) override
  {
    std::cout << "Observer got notified: " << msg << "\n";
  }
};

//int main(int argc, char** argv)
int main()
{
  ThreadPool thread_pool{1};

  std::vector<std::future<std::string>> FutureVec;

  Observer obs;
  auto msg = std::make_shared<DummyMessage>(&thread_pool, 5);
  msg->Attach(&obs);

  FutureVec.emplace_back(msg->get_future());
  thread_pool.submit(msg);

  auto it = FutureVec.begin();

  std::this_thread::sleep_for(std::chrono::seconds{1});

  std::cout << FutureVec.size() << std::endl;

  while(!FutureVec.empty())
  {
    if(it->valid())
    {
      std::cout << it->get() << std::endl;
      FutureVec.erase(it);
      if(it == FutureVec.end())
      {
        break;
      }

      continue;
    }

    ++it;

    if(it == FutureVec.end())
    {
      it = FutureVec.begin();
    }
  }

  std::cout << "fin" << std::endl;
  return 0;
//
//    po::options_description description("peerpaste, a chord based p2p pastebin");
//
//    description.add_options()
//        ("help,h", "Display help message")
//        ("port,p", po::value<unsigned>(), "Port to listen on")
//        ("join,j", po::value<std::vector<std::string>>()->multitoken()->composing(), "IP and Port of Host to connect to")
//        ("put", po::value<std::vector<std::string>>()->multitoken()->composing(), "Path to text file")
//        ("get", po::value<std::vector<std::string>>()->multitoken()->composing(), "Hash of a stored Paste")
//        ("put-unencrypted", po::value<std::vector<std::string>>()->multitoken()->composing(), "Path to text file")
//        ("get-unencrypted", po::value<std::vector<std::string>>()->multitoken()->composing(), "Hash of a stored Paste")
//        ("verbose", "show additional information")
//        ("create", "create new ring")
//        ("log-messages", "log messages to files")
//        ("debug", "Send routing information to localhost");
//
//    po::variables_map vm;
//    try
//    {
//        po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
//        po::notify(vm);
//
//        if (vm.count("help")) {
//            std::cout << description;
//            return 0;
//        }
//
//        /////////////////////////////LOGGING
//        if(vm.count("verbose")){
//            logging::add_console_log(std::clog,
//                                     keywords::filter = expr::attr< severity_level >("Severity") <= debug,
//                                     keywords::format = expr::stream
//                    << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%m-%d, %H:%M:%S.%f")
//                    /* << " [" << expr::format_date_time< attrs::timer::value_type >("Uptime", "%O:%M:%S") */
//                    << "[" << std::this_thread::get_id()
//                    /* << "] [" << expr::format_named_scope("Scope", keywords::format = "%n (%f:%l)") */
//                    << "] <" << expr::attr< severity_level >("Severity")
//                    << "> " << expr::message
//            );
//        } else {
//            logging::add_console_log(std::clog,
//                                     keywords::filter = expr::attr< severity_level >("Severity") <= info,
//                                     keywords::format = expr::stream
//                    << expr::message
//            );
//        }
//
//        if (vm.count("log-messages")){
//            // One can also use lambda expressions to setup filters and formatters
//            logging::add_file_log
//            (
//                "outgoing_message.log",
//                keywords::filter = expr::attr<severity_level>("Severity") == message_out,
//                keywords::format = expr::stream
//                    << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d, %H:%M:%S.%f")
//                    << " [" << expr::format_date_time< attrs::timer::value_type >("Uptime", "%O:%M:%S")
//                    << "] [" << expr::format_named_scope("Scope", keywords::format = "%n (%f:%l)")
//                    << "] <" << expr::attr< severity_level >("Severity")
//                    << ">\n"
//                    << expr::message
//            );
//            logging::add_file_log
//            (
//                "ingoing_message.log",
//                keywords::filter = expr::attr<severity_level>("Severity") == message_in,
//                keywords::format = expr::stream
//                    << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d, %H:%M:%S.%f")
//                    << " [" << expr::format_date_time< attrs::timer::value_type >("Uptime", "%O:%M:%S")
//                    << "] [" << expr::format_named_scope("Scope", keywords::format = "%n (%f:%l)")
//                    << "] <" << expr::attr< severity_level >("Severity")
//                    << ">\n"
//                    << expr::message
//            );
//        }
//
//        // Also let's add some commonly used attributes, like timestamp and record counter.
//        logging::add_common_attributes();
//        logging::core::get()->add_thread_attribute("Scope", attrs::named_scope());
//        BOOST_LOG_FUNCTION();
//        /* util::log(info, "info message"); */
//        /* util::log(error, "error message"); */
//        /* util::log(message_out, "Outgoing message"); */
//        /* util::log(message_in, "Ingoing message"); */
//        /////////////////////////////LOGGING
//
//        peerpaste::PeerPaste peerpaste;
//
//        if (vm.count("port")) {
//            peerpaste.init(vm["port"].as<unsigned>(), 4);
//        }
//        else {
//            util::log(info, "You have to specify a port using the --port option");
//            return 0;
//        }
//
//        if (vm.count("join")) {
//            //TODO: add a lot of boundary checking
//            auto vec = vm["join"].as<std::vector<std::string>>();
//            auto host_ip = vec.at(0);
//            auto host_port = vec.at(1);
//            peerpaste.join(host_ip, host_port);
//        } else if(vm.count("create")){
//            peerpaste.create_ring();
//        }
//
//        //TODO: integrate Server int MessageDispatcher and start it by fct like
//        //      msg_dispatcher->listen(uint16_t port)
//        /* server->run(); */
//        /* msg_dispatcher->run(); */
//
//        if(vm.count("debug")) {
//            peerpaste.debug();
//        }
//        peerpaste.run();
//
//        if(vm.count("put")) {
//            auto vec = vm["put"].as<std::vector<std::string>>();
//            auto ip = vec.at(0);
//            auto port = vec.at(1);
//            std::string filename = vec.at(2);
//            std::ifstream f(filename);
//            std::string data((std::istreambuf_iterator<char>(f)),
//                             std::istreambuf_iterator<char>());
//            auto data_key = util::generate_sha256(data, "");
//            auto data_encrypted = util::encrypt(data_key, data);
//            auto future_ = peerpaste.async_put(ip, port, data_encrypted);
//            future_.wait();
//            std::cout << data_key << future_.get() << std::endl;
//        } else if(vm.count("put-unencrypted")) {
//            auto vec = vm["put-unencrypted"].as<std::vector<std::string>>();
//            auto ip = vec.at(0);
//            auto port = vec.at(1);
//            std::string filename = vec.at(2);
//            std::ifstream f(filename);
//            std::string data((std::istreambuf_iterator<char>(f)),
//                             std::istreambuf_iterator<char>());
//            auto future_ = peerpaste.async_put(ip, port, data);
//            future_.wait();
//            std::cout << future_.get() << std::endl;
//        } else if(vm.count("get")) {
//            auto vec = vm["get"].as<std::vector<std::string>>();
//            auto ip = vec.at(0);
//            auto port = vec.at(1);
//            auto data = vec.at(2);
//            auto data_key = data.substr(0, 64);
//            auto data_hash = data.substr(64, 64);
//            auto future_ = peerpaste.async_get(ip, port, data_hash);
//            future_.wait();
//            std::cout << util::decrypt(data_key, future_.get()) << std::endl;
//        } else if(vm.count("get-unencrypted")) {
//            auto vec = vm["get-unencrypted"].as<std::vector<std::string>>();
//            auto ip = vec.at(0);
//            auto port = vec.at(1);
//            auto data = vec.at(2);
//            auto future_ = peerpaste.async_get(ip, port, data);
//            future_.wait();
//            std::cout << future_.get() << std::endl;
//        } else {
//            //only call msg_handler.run() when this node
//            //should be part of the ring
//            peerpaste.wait_till_finish();
//        }
//    }
//    catch (const po::error& ex) {
//        return -1;
//    }
//
//    return 0;
}


//std::atomic<int> seconds = 0;
//std::mutex mutex_;
//
//void calculation()
//{
//  std::unique_lock<std::mutex> lk(mutex_);
//  const auto secs = ++seconds;
//  lk.unlock();
//
//  std::this_thread::sleep_for(std::chrono::seconds(secs));
//  std::cout << std::this_thread::get_id() << " waited " << secs << " seconds" << std::endl;
//}
//
