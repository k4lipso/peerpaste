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

    /* std::cout << sizeof(char) << std::endl; */
    /* std::cout << sizeof(char[8]) << std::endl; */
    /* std::cout << sizeof(char[32]) << std::endl; */
    /* std::cout << sizeof(bool) << std::endl; */
    /* std::cout << sizeof(int8_t) << std::endl; */
    /* std::cout << sizeof(int16_t) << std::endl; */
    /* std::cout << sizeof(int32_t) << std::endl; */
    /* std::cout << sizeof(int64_t) << std::endl; */
    /* std::cout << sizeof(size_t) << std::endl; */
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

