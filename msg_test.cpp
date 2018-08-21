// blocking_tcp_echo_client.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <boost/asio.hpp>

#include <google/protobuf/util/delimited_message_util.h>

#include "proto/messages.pb.h"

using boost::asio::ip::tcp;

enum { max_length = 100024 };

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: blocking_tcp_echo_client <host> <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    tcp::socket s(io_context);
    tcp::resolver resolver(io_context);
    boost::asio::connect(s, resolver.resolve(argv[1], argv[2]));

    /* std::string str = "fa130asl2lakdj109id"; */
    /* boost::asio::write(s, boost::asio::buffer(str, sizeof(str))); */

    CommonHeader header;
    header.set_t_flag(true);
    header.set_ttl(1);
    header.set_message_length(16);
    header.set_request_type("join");
    header.set_transaction_id("dakjh2(1s8d1)");
    header.set_version("1.0.9");

    boost::asio::streambuf stream_buffer;
    std::ostream output_stream(&stream_buffer);
    google::protobuf::util::SerializeDelimitedToOstream(header, &output_stream);
    /* std::istream response_stream(&stream_buffer); */
    /* std::string data(std::istreambuf_iterator<char>(response_stream), {}); */
    /* std::cout << data << std::endl; */

    boost::asio::write(s, stream_buffer);

    header.set_t_flag(false);
    header.set_ttl(13);
    header.set_message_length(61);
    header.set_request_type("hover, my friend");
    header.set_transaction_id("dal209falk2h9dadkl1khe1kldh");
    header.set_version("1.1.3");

    /* boost::asio::streambuf stream_buffer; */
    /* std::ostream output_stream(&stream_buffer); */
    google::protobuf::util::SerializeDelimitedToOstream(header, &output_stream);
    /* std::istream response_stream(&stream_buffer); */
    /* std::string data(std::istreambuf_iterator<char>(response_stream), {}); */
    /* std::cout << data << std::endl; */

    boost::asio::write(s, stream_buffer);

    /* std::string str(std::istreambuf_iterator<char>(stream_buffer), {}); */

    /* boost::asio::write(s, boost::asio::buffer(str, str.size())); */

    /* char reply[8]; */
    /* size_t reply_length = boost::asio::read(s, */
    /*     boost::asio::buffer(reply, 8)); */
    /* std::cout << "Reply is: "; */
    /* std::cout.write(reply, reply_length); */
    /* std::cout << "\n"; */
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

