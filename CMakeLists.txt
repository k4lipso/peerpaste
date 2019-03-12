cmake_minimum_required (VERSION 3.2.2)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
# SET(CMAKE_CXX_FLAGS  "-Q")

project("PeerPaste" CXX)

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
FIND_PACKAGE(Boost REQUIRED COMPONENTS log system thread regex)
MESSAGE("Boost information")
MESSAGE("Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
MESSAGE("Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
MESSAGE("Boost Libraries: ${Boost_LIBRARIES}")
MESSAGE("Boost Libraries: ${Boost_LOG_LIBRARY}")
MESSAGE("PROTOBUF INCLUDE DIRS ${PROTOBUF_INCLUDE_DIRS}")
MESSAGE("PROTOBUF LIBRARIES ${PROTOBUF_LIBRARIES}")
# IF(Boost_FOUND)
#   INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
#   LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
# ENDIF(Boost_FOUND)

# SET(USED_LIBS  ${Boost_LOG_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_REGEX_LIBRARY})

add_library(libserver
    "peer.hpp"
    "header.hpp"
    "message.hpp"
    "session.hpp"
    "message_converter.hpp"
    "message_queue.cpp"
    "write_queue.cpp"
    "session.cpp"
    "server.hpp"
    "routing_table.hpp"
    "message_handler.hpp"
    "aggregator.hpp"
    "aggregat.hpp"
    "cryptowrapper.cpp"
    "proto/messages.pb.cc"
    )

set_target_properties(libserver PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(libserver
    PUBLIC
        # ${BOOST_INCLUDE_DIRS}
        # ${PROTOBUF_INCLUDE_DIRS}
        ${CRYPTOPP_INCLUDE_DIR}
        "/usr/include/cryptopp"
    )

target_link_libraries(libserver
    PUBLIC
        ${CMAKE_THREAD_LIBS_INIT}
        ${Boost_LIBRARIES}
        ${Boost_LIBRARY_DIRS}
        ${PROTOBUF_LIBRARIES}
        ${CRYPTOPP_LIBRARIES}
    )

add_executable(node main.cpp)

target_include_directories(node
    PUBLIC
        # ${BOOST_INCLUDE_DIRS}
)

target_link_libraries(node
    PUBLIC
        libserver
)

add_executable(peer main2.cpp)

target_include_directories(peer
    PUBLIC
)

target_link_libraries(peer
    PUBLIC
        libserver
)

# add_executable(msg_test msg_test.cpp)

# target_include_directories(msg_test
#     PUBLIC
#         # ${BOOST_INCLUDE_DIRS}
# )

# target_link_libraries(msg_test
#     PUBLIC
#         libserver
# )

add_subdirectory(test)
# find_package(Catch2 REQUIRED)

# add_executable(test test.cpp)

# target_include_directories(test
#     PUBLIC
#         ./
#         # ${BOOST_INCLUDE_DIRS}
#         # ${PROTOBUF_INCLUDE_DIRS}
#         # ${CRYPTOPP_INCLUDE_DIR}
#         # "/usr/include/cryptopp"
# )

# target_link_libraries(test
#     PUBLIC
#         # ${CMAKE_THREAD_LIBS_INIT}
#         # ${Boost_LIBRARIES}
#         # # ${USED_LIBS}
#         # ${Boost_LIBRARY_DIRS}
#         # ${PROTOBUF_LIBRARIES}
#         # ${CRYPTOPP_LIBRARIES}
#         libserver
# )
