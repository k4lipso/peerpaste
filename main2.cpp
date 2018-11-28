#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>
#include <utility>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/array.hpp>

#include "server.hpp"


int main(int argc, char* argv[])
{
  try
  {
    if (argc != 4)
    {
      std::cerr << "Usage: chat_client <node_addr> <node_port> <own_port>\n";
      return 1;
    }

    boost::asio::io_context io_context;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> foo
        = boost::asio::make_work_guard(io_context);

    server s;
    s.start_client(argv[1], std::stoi(argv[2]), std::atoi(argv[3]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

