cmake_minimum_required (VERSION 3.2.2)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Debug)
SET(CMAKE_CXX_FLAGS  "-g -O0")

project("PeerPaste" CXX)

option(BUILD_TESTS "test will be build" OFF)
option(BUILD_DOC "build documentation" OFF)
option(SANITIZE "address sanitizers" OFF)

find_package (Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(CryptoPP REQUIRED)
find_package(sqlite3 REQUIRED)

add_subdirectory(include/peerpaste/proto)

SET(Boost_USE_MULTITHREAD ON)
FIND_PACKAGE(Boost 1.71.0 REQUIRED COMPONENTS log_setup log program_options system thread regex filesystem)

message("Boost information")
message("Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message("Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
message("Boost Libraries: ${Boost_LIBRARIES}")
message("PROTOBUF INCLUDE DIRS ${PROTOBUF_INCLUDE_DIRS}")
message("PROTOBUF LIBRARIES ${PROTOBUF_LIBRARIES}")

add_library(libpeerpaste
    "include/peerpaste/peerpaste.hpp"
    "include/peerpaste/peer.hpp"
    "include/peerpaste/header.hpp"
    "include/peerpaste/message.hpp"
    "src/boost_session.cpp"
    "include/peerpaste/message_converter.hpp"
    "include/peerpaste/server.hpp"
    "include/peerpaste/message_handler.hpp"
    "include/peerpaste/request_object.hpp"
    "src/cryptowrapper.cpp"
    "src/thread_pool.cpp"
    "src/messaging_base.cpp"
    "src/observable.cpp"
    "src/observer_base.cpp"
    "src/notify.cpp"
    "src/check_predecessor.cpp"
    "src/find_successor.cpp"
    "src/query.cpp"
    "src/join.cpp"
    "src/get_successor_list.cpp"
    "src/get_self_and_succ_list.cpp"
    "src/get_pred_and_succ_list.cpp"
    "src/stabilize.cpp"
    "src/broadcast_file_list.cpp"
    "src/get_file.cpp"
    "src/message_factory.cpp"
    "src/concurrent_request_handler.cpp"
    "src/concurrent_set.cpp"
    "src/storage.cpp"
    "include/peerpaste/concurrent_queue.hpp"
    "include/peerpaste/concurrent_routing_table.hpp"
    "include/peerpaste/consumer.hpp"
    "include/peerpaste/concurrent_request_handler.hpp"
    )

set_target_properties(libpeerpaste PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(libpeerpaste
    PUBLIC
        "external/sqlite_modern_cpp/hdr"
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${Boost_INCLUDE_DIRS}
        ${CRYPTOPP_INCLUDE_DIR}
	      "${CMAKE_CURRENT_BINARY_DIR}/include/peerpaste"
    )

target_link_libraries(libpeerpaste
    PUBLIC
        ${CMAKE_THREAD_LIBS_INIT}
        Boost::log_setup
        Boost::log
        Boost::system
        Boost::thread
        Boost::regex
        Boost::filesystem
	      proto
        ${PROTOBUF_LIBRARIES}
        ${CRYPTOPP_LIBRARIES}
        ${SQLITE3_LIBRARY}
        -lsodium
    )

  target_compile_options(libpeerpaste
    PUBLIC
    -DBOOST_LOG_DYN_LINK -ggdb
    )

add_executable(peerpaste src/main.cpp)

target_include_directories(peerpaste
    PUBLIC
)

target_link_libraries(peerpaste
    PUBLIC
        Boost::log_setup
        Boost::log
        Boost::program_options
        libpeerpaste
)

target_compile_options(peerpaste
  PUBLIC
  -DBOOST_LOG_DYN_LINK
  )

if(BUILD_TESTS)
    add_subdirectory(test)
endif()


set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U_FORTIFY_SOURCE -O0 -Wall -Wextra -g3 -fno-omit-frame-pointer")

if(SANITIZE)
  message("compiling with address sanitzers...")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

if(BUILD_DOC)
  # look for Doxygen package
  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    message("Found Doxygen")
    # exclude sqlite code
    set(DOXYGEN_EXCLUDE_PATTERNS
      */sqlite3/*
      )
    # doxygen settings can be set here, prefixed with "DOXYGEN_"
    set(DOXYGEN_SOURCE_BROWSER YES)
    set(DOXYGEN_EXTRACT_PRIVATE YES)
    set(DOXYGEN_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/api-docs")
    # this target will only be built if specifically asked to.
    # run "make api-docs" to create the doxygen documentation
    doxygen_add_docs(
      api-docs
      ${PROJECT_SOURCE_DIR}
      COMMENT "Generate API-documents for NoteSearch."
      )
  endif(DOXYGEN_FOUND)
endif()

message("End")
