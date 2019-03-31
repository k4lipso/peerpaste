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
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    /* boost::asio::io_context io_context; */
    boost::asio::io_context io_context;
    auto foo = boost::asio::make_work_guard(io_context);

    Server s(4, std::atoi(argv[1]));
    s.start_server();

    io_context.run();
    std::cout << "CLEANING UP" << std::endl;
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  std::cout << "FINISH" << std::endl;
  return 0;
}


