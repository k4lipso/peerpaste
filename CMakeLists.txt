cmake_minimum_required (VERSION 3.2.2)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
# SET(CMAKE_CXX_FLAGS  "-Q")

project("PeerPaste" CXX)

option(BUILD_TESTS "test will be build" OFF)
option(DEBUG "sanitizers activated" OFF)

find_package (Threads REQUIRED)

find_package(Protobuf REQUIRED)
# add_subdirectory(proto)

find_package(CryptoPP REQUIRED)
message("${CRYPTOPP_INCLUDE_DIR}")
message("${CRYPTOPP_INCLUDE_DIRS}")
message("${CRYPTOPP_ROOT_DIR}")
message("${CRYPTOPP_FOUND}")

SET(Boost_USE_STATIC_LIBS ON)
SET(Boost_USE_MULTITHREAD ON)
FIND_PACKAGE(Boost REQUIRED COMPONENTS program_options log system thread regex)
MESSAGE("Boost information")
MESSAGE("Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
MESSAGE("Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
MESSAGE("Boost Libraries: ${Boost_LIBRARIES}")
MESSAGE("Boost Libraries: ${Boost_LOG_LIBRARY}")
MESSAGE("PROTOBUF INCLUDE DIRS ${PROTOBUF_INCLUDE_DIRS}")
MESSAGE("PROTOBUF LIBRARIES ${PROTOBUF_LIBRARIES}")

# Add sanitizer

add_library(libpeerpaste
    "include/peerpaste/peer.hpp"
    "include/peerpaste/header.hpp"
    "include/peerpaste/message.hpp"
    "src/boost_session.cpp"
    "include/peerpaste/message_converter.hpp"
    "include/peerpaste/server.hpp"
    "include/peerpaste/message_handler.hpp"
    "src/aggregator.cpp"
    "src/aggregat.cpp"
    "src/cryptowrapper.cpp"
    "include/peerpaste/storage.hpp"
    "include/peerpaste/proto/messages.pb.cc"
    "include/peerpaste/concurrent_queue.hpp"
    "include/peerpaste/concurrent_routing_table.hpp"
    "include/peerpaste/consumer.hpp"
    "include/peerpaste/concurrent_request_handler.hpp"
    )

set_target_properties(libpeerpaste PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(libpeerpaste
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${CRYPTOPP_INCLUDE_DIR}
        "/usr/include/cryptopp"
    )

target_link_libraries(libpeerpaste
    PUBLIC
        ${CMAKE_THREAD_LIBS_INIT}
        ${Boost_LIBRARIES}
        ${Boost_LIBRARY_DIRS}
        ${PROTOBUF_LIBRARIES}
        ${CRYPTOPP_LIBRARIES}
        -lsodium
    )

add_executable(peerpaste src/peerpaste.cpp)

target_include_directories(peerpaste
    PUBLIC
)

target_link_libraries(peerpaste
    PUBLIC
        ${Boost_LIBRARIES}
        libpeerpaste
        # if(DEBUG)
            # -fsanitize=address
        # endif()
)

if(BUILD_TESTS)
    add_subdirectory(test)
endif()
if(DEBUG)
    add_definitions(-Wall -fsanitize=address -g3 -fno-omit-frame-pointer)
endif()
