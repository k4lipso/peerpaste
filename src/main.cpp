#include <chrono>
#include <cstdlib>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/array.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>

#include "peerpaste/cryptowrapper.hpp"
#include "peerpaste/messaging_base.hpp"
#include "peerpaste/peerpaste.hpp"
#include "peerpaste/server.hpp"
#include "peerpaste/thread_pool.hpp"

namespace po = boost::program_options;

int main(int argc, char **argv)
{

	po::options_description description("peerpaste, a chord based p2p pastebin");

	description.add_options()(
		"help,h", "Display help message")(
		"address,a", po::value<std::string>(), "Address to listen on")(
		"port,p", po::value<unsigned>(), "Port to listen on")(
		"join,j", po::value<std::vector<std::string>>()->multitoken()->composing(), "IP and Port of Host to connect to")(
		"verbose", "show additional information")("create", "create new ring")("log-messages", "log messages to files")(
		"debug", "Send routing information to localhost");

	po::variables_map vm;
	try
	{
		po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
		po::notify(vm);

		if(vm.count("help"))
		{
			std::cout << description;
			return 0;
		}

		/////////////////////////////LOGGING
		if(vm.count("verbose"))
		{
			logging::add_console_log(
				std::clog,
				keywords::filter = expr::attr<severity_level>("Severity") <= debug,
				keywords::format =
					expr::stream << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%m-%d, %H:%M:%S.%f")
											 /* << " [" << expr::format_date_time< attrs::timer::value_type >("Uptime", "%O:%M:%S") */
											 << "["
											 << std::this_thread::get_id()
											 /* << "] [" << expr::format_named_scope("Scope", keywords::format = "%n (%f:%l)") */
											 << "] <" << expr::attr<severity_level>("Severity") << "> " << expr::message);

				spdlog::set_level(spdlog::level::debug);
		}
		else
		{
			logging::add_console_log(std::clog,
															 keywords::filter = expr::attr<severity_level>("Severity") <= info,
															 keywords::format = expr::stream << expr::message);
		}

		if(vm.count("log-messages"))
		{
			// One can also use lambda expressions to setup filters and formatters
			logging::add_file_log("outgoing_message.log",
														keywords::filter = expr::attr<severity_level>("Severity") == message_out,
														keywords::format =
															expr::stream
															<< expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d, %H:%M:%S.%f")
															<< " [" << expr::format_date_time<attrs::timer::value_type>("Uptime", "%O:%M:%S") << "] ["
															<< expr::format_named_scope("Scope", keywords::format = "%n (%f:%l)") << "] <"
															<< expr::attr<severity_level>("Severity") << ">\n"
															<< expr::message);
			logging::add_file_log("ingoing_message.log",
														keywords::filter = expr::attr<severity_level>("Severity") == message_in,
														keywords::format =
															expr::stream
															<< expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d, %H:%M:%S.%f")
															<< " [" << expr::format_date_time<attrs::timer::value_type>("Uptime", "%O:%M:%S") << "] ["
															<< expr::format_named_scope("Scope", keywords::format = "%n (%f:%l)") << "] <"
															<< expr::attr<severity_level>("Severity") << ">\n"
															<< expr::message);
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

		if(vm.count("port") && vm.count("address"))
		{
			peerpaste.init(vm["address"].as<std::string>(), vm["port"].as<unsigned>(), 4);
		}
		else
		{
			peerpaste.wait_till_finish();
			spdlog::info("You have to specify address and port using the --address,-a and --port,-p option");
			return 0;
		}

		if(vm.count("join"))
		{
			// TODO: add a lot of boundary checking
			auto vec = vm["join"].as<std::vector<std::string>>();
			auto host_ip = vec.at(0);
			auto host_port = vec.at(1);
			peerpaste.join(host_ip, host_port);
		}
		else if(vm.count("create"))
		{
			peerpaste.create_ring();
		}

		// TODO: integrate Server int MessageDispatcher and start it by fct like
		//      msg_dispatcher->listen(uint16_t port)
		/* server->run(); */
		/* msg_dispatcher->run(); */

		if(vm.count("debug"))
		{
			peerpaste.debug();
		}
		peerpaste.run();

		// only call msg_handler.run() when this node
		// should be part of the ring
		peerpaste.wait_till_finish();
	}
	catch(const po::error &ex)
	{
		return -1;
	}

	return 0;
}
