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
        ("put", po::value<std::vector<std::string>>()->multitoken()->composing(), "Path to text file")
        ("get", po::value<std::vector<std::string>>()->multitoken()->composing(), "Hash of a stored Paste")
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
            auto vec = vm["put"].as<std::vector<std::string>>();
            std::cout << vec.size() << std::endl;
            auto ip = vec.at(0);
            auto port = vec.at(1);
            std::ifstream t(vec.at(2));
            std::string str((std::istreambuf_iterator<char>(t)),
                             std::istreambuf_iterator<char>());
            msg_handler->put(ip, port, str);
        } else if(vm.count("get")) {
            auto vec = vm["get"].as<std::vector<std::string>>();

            auto ip = vec.at(0);
            auto port = vec.at(1);
            auto data = vec.at(2);
            msg_handler->get(ip, port, data);
        } else {
            //only call msg_handler.run() when this node
            //should be part of the ring
            std::thread t3([&]() { msg_handler->run(); });
            t3.detach();
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

