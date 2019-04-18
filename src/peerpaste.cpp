#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <fstream>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/program_options.hpp>

#include "peerpaste/server.hpp"

namespace po = boost::program_options;

int main(int argc, char** argv)
{

    po::options_description description("peerpaste, a chord based p2p pastebin");

    description.add_options()
        ("help,h", "Display help message")
        ("port,p", po::value<unsigned>(), "Port to listen on")
        ("join,j", po::value<std::vector<std::string>>()->multitoken()->composing(), "IP and Port of Host to connect to")
        ("put", po::value<std::string>(), "Path to text file")
        ("get", po::value<std::string>(), "Hash of a stored Paste")
        ("debug", "Send routing information to localhost");

    po::variables_map vm;
    boost::asio::io_context io_context;
    auto foo = boost::asio::make_work_guard(io_context);
    std::unique_ptr<Server> server = nullptr;
    std::unique_ptr<peerpaste::MessageDispatcher> msg_dispatcher = nullptr;
    std::shared_ptr<MessageHandler> msg_handler = nullptr;

    try
    {
        po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << description;
            return 0;
        }

        if (vm.count("port")) {
            msg_handler = std::make_shared<MessageHandler>(vm["port"].as<unsigned>());
            msg_dispatcher = std::make_unique<peerpaste::MessageDispatcher>(msg_handler, io_context);
            msg_handler->init(msg_dispatcher->get_send_queue());
            server = std::make_unique<Server>(4,
                                              vm["port"].as<unsigned>(),
                                              msg_dispatcher->get_receive_queue());
        }
        else {
            std::cout << "You have to specify a port using the --port option\n";
            return 0;
        }

        server->run();

        if (vm.count("join")) {
            //TODO: add a lot of boundary checking
            auto vec = vm["join"].as<std::vector<std::string>>();
            auto host_ip = vec.at(0);
            auto host_port = vec.at(1);
            msg_handler->join(host_ip, host_port);
        }
        msg_dispatcher->run();

        if (vm.count("put")) {
            std::ifstream t(vm["put"].as<std::string>());
            std::string str((std::istreambuf_iterator<char>(t)),
                             std::istreambuf_iterator<char>());
            /* server->put(str); */
        }

        if (vm.count("get")) {
            std::string data_hash = vm["get"].as<std::string>();
            /* server->get(data_hash); */
        }

        if (vm.count("debug")) {
            /* server->send_routing_information(true); */
        }
    }
    catch (const po::error& ex) {
        return -1;
    }

    try
    {
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    std::cout << "FINISH" << std::endl;
    return 0;
}

