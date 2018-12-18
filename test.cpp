#include "routingTable.hpp"
#include "server.hpp"
#include "session.hpp"
#include "peer.hpp"
#include "message.hpp"
#include "proto/messages.pb.h"
#include <iostream>
#include <string>
#include <memory>

#include <boost/asio.hpp>
#include <boost/array.hpp>

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include <catch.hpp>

TEST_CASE( "Testing util::between()", "[util::between()]" ) {
    std::string a = "a";
    std::string b = "b";
    std::string c = "c";
    std::string num1 = "1";
    std::string num2 = "2";
    std::string num3 = "3";

    REQUIRE(util::between(a, b, c) == true);
    REQUIRE(util::between(b, b, c) == false);
    REQUIRE(util::between(c, b, c) == true);
    REQUIRE(util::between(a, b, a) == true);
    REQUIRE(util::between(c, a, b) == true);
    REQUIRE(util::between(num2, b, num1) == true);
    REQUIRE(util::between(num2, b, num3) == false);
    REQUIRE(util::between(num1, num2, num3) == true);
    REQUIRE(util::between(num3, num2, num1) == false);
    REQUIRE(util::between(num3, c, num2) == true);
    REQUIRE(util::between(c, num1, a) == true);
    REQUIRE(util::between(num1, num3, c) == true);
}
