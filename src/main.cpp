#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <fstream>

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/array.hpp>
#include <boost/program_options.hpp>

#include "peerpaste/cryptowrapper.hpp"
#include "peerpaste/server.hpp"
#include "peerpaste/peerpaste.hpp"

namespace po = boost::program_options;

int main(int argc, char** argv)
{

    po::options_description description("peerpaste, a chord based p2p pastebin");

    description.add_options()
        ("help,h", "Display help message")
        ("port,p", po::value<unsigned>(), "Port to listen on")
        ("join,j", po::value<std::vector<std::string>>()->multitoken()->composing(), "IP and Port of Host to connect to")
        ("put", po::value<std::vector<std::string>>()->multitoken()->composing(), "Path to text file")
        ("get", po::value<std::vector<std::string>>()->multitoken()->composing(), "Hash of a stored Paste")
        ("verbose", "show additional information")
        ("create", "create new ring")
        ("log-messages", "log messages to files")
        ("debug", "Send routing information to localhost");

    po::variables_map vm;
    try
    {
        po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << description;
            return 0;
        }

        /////////////////////////////LOGGING
        if(vm.count("verbose")){
            logging::add_console_log(std::clog,
                                     keywords::filter = expr::attr< severity_level >("Severity") <= debug,
                                     keywords::format = expr::stream
                    << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%m-%d, %H:%M:%S.%f")
                    /* << " [" << expr::format_date_time< attrs::timer::value_type >("Uptime", "%O:%M:%S") */
                    << "[" << std::this_thread::get_id()
                    /* << "] [" << expr::format_named_scope("Scope", keywords::format = "%n (%f:%l)") */
                    << "] <" << expr::attr< severity_level >("Severity")
                    << "> " << expr::message
            );
        } else {
            logging::add_console_log(std::clog,
                                     keywords::filter = expr::attr< severity_level >("Severity") <= info,
                                     keywords::format = expr::stream
                    << expr::message
            );
        }

        if (vm.count("log-messages")){
            // One can also use lambda expressions to setup filters and formatters
            logging::add_file_log
            (
                "outgoing_message.log",
                keywords::filter = expr::attr<severity_level>("Severity") == message_out,
                keywords::format = expr::stream
                    << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d, %H:%M:%S.%f")
                    << " [" << expr::format_date_time< attrs::timer::value_type >("Uptime", "%O:%M:%S")
                    << "] [" << expr::format_named_scope("Scope", keywords::format = "%n (%f:%l)")
                    << "] <" << expr::attr< severity_level >("Severity")
                    << ">\n"
                    << expr::message
            );
            logging::add_file_log
            (
                "ingoing_message.log",
                keywords::filter = expr::attr<severity_level>("Severity") == message_in,
                keywords::format = expr::stream
                    << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d, %H:%M:%S.%f")
                    << " [" << expr::format_date_time< attrs::timer::value_type >("Uptime", "%O:%M:%S")
                    << "] [" << expr::format_named_scope("Scope", keywords::format = "%n (%f:%l)")
                    << "] <" << expr::attr< severity_level >("Severity")
                    << ">\n"
                    << expr::message
            );
        }

        // Also let's add some commonly used attributes, like timestamp and record counter.
        logging::add_common_attributes();
        logging::core::get()->add_thread_attribute("Scope", attrs::named_scope());
        BOOST_LOG_FUNCTION();
        /* util::log(info, "info message"); */
        /* util::log(error, "error message"); */
        /* util::log(message_out, "Outgoing message"); */
        /* util::log(message_in, "Ingoing message"); */
        /////////////////////////////LOGGING

        peerpaste::PeerPaste peerpaste;

        if (vm.count("port")) {
            peerpaste.init(vm["port"].as<unsigned>(), 4);
        }
        else {
            util::log(info, "You have to specify a port using the --port option");
            return 0;
        }

        //TODO: message_dispatcher should call io_context.run() instead of server
        //for get/put request no server is needed at all, so msg_dispatcher should be independend
        //TODO: call server->run() only when no put/get
        /* server->run(); */

        if (vm.count("join")) {
            //TODO: add a lot of boundary checking
            auto vec = vm["join"].as<std::vector<std::string>>();
            auto host_ip = vec.at(0);
            auto host_port = vec.at(1);
            peerpaste.join(host_ip, host_port);
        } else if(vm.count("create")){
            peerpaste.create_ring();
        }

        //TODO: integrate Server int MessageDispatcher and start it by fct like
        //      msg_dispatcher->listen(uint16_t port)
        /* server->run(); */
        /* msg_dispatcher->run(); */

        if(vm.count("debug")) {
            peerpaste.debug();
        }
        peerpaste.run();

        if(vm.count("put")) {
            auto vec = vm["put"].as<std::vector<std::string>>();
            auto ip = vec.at(0);
            auto port = vec.at(1);
            std::string filename = vec.at(2);
            auto future_ = peerpaste.async_put(ip, port, filename);
            future_.wait();
            std::cout << future_.get() << std::endl;
        } else if(vm.count("get")) {
            auto vec = vm["get"].as<std::vector<std::string>>();
            auto ip = vec.at(0);
            auto port = vec.at(1);
            auto data = vec.at(2);
            auto future_ = peerpaste.async_get(ip, port, data);
            future_.wait();
            std::cout << future_.get() << std::endl;
        } else {
            //only call msg_handler.run() when this node
            //should be part of the ring
            peerpaste.wait_till_finish();
        }
        peerpaste.stop();
    }
    catch (const po::error& ex) {
        return -1;
    }

    return 0;
}

