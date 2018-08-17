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

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

