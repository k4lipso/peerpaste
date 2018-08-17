cmake_minimum_required (VERSION 3.2.2)

project("PeerPaste" CXX)

find_package (Threads)

SET(Boost_USE_STATIC_LIBS OFF)
SET(Boost_USE_MULTITHREAD ON)
FIND_PACKAGE(Boost REQUIRED COMPONENTS log system thread regex)
MESSAGE("Boost information")
MESSAGE("Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
MESSAGE("Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
MESSAGE("Boost Libraries: ${Boost_LIBRARIES}")
# IF(Boost_FOUND)
#   INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
#   LINK_DIRECTORIES(${Boost_LIBRARY_DIRS})
# ENDIF(Boost_FOUND)

# SET(USED_LIBS ${Boost_LIBRARIES} ${Boost_LIBRARIES} ${Boost_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_REGEX_LIBRARY})

add_library(libserver
    "server.hpp"
    "routingTable.hpp"
    "session.hpp"
    "peer.hpp"
    "message.hpp"
    )

set_target_properties(libserver PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(libserver
    PUBLIC
        ./
        ${BOOST_INCLUDE_DIRS}
    )

target_link_libraries(libserver
    PUBLIC
        ${Boost_LIBRARIES}
        ${Boost_LIBRARY_DIRS}
        ${CMAKE_THREAD_LIBS_INIT}
    )

add_executable(node main.cpp)

target_include_directories(node
    PUBLIC
        ./
        # ${BOOST_INCLUDE_DIRS}
)

target_link_libraries(node
    PUBLIC
        # ${Boost_LIBRARIES}
        # ${Boost_LIBRARY_DIRS}
        # ${CMAKE_THREAD_LIBS_INIT}
        libserver
)
