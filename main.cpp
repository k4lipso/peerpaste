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
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> foo
        = boost::asio::make_work_guard(io_context);

    server s;
    s.start_server(1337);

    /* std::thread thread([&io_context](){ io_context.run(); }); */
    /* thread.join(); */
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

