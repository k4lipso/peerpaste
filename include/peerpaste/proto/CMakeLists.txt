
INCLUDE(FindProtobuf)
FIND_PACKAGE(Protobuf REQUIRED)
INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER messages.proto)
ADD_LIBRARY(proto ${PROTO_HEADER} ${PROTO_SRC})

#include(FindProtobuf)
#find_package(Protobuf REQUIRED)
#
##include_directories(${PROTOBUF_INCLUDE_DIR})
##PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER message.proto)
##
##add_library(proto ${PROTO_HEADER} ${PROTO_SRC})
##target_include_directories(proto
##    PRIVATE
##        ${PROTOBUF_INCLUDE_DIR}
##    )
#
#set(PROTOS
#	${CMAKE_CURRENT_SOURCE_DIR}/messages.proto
#)
#
#set(PROTO_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto)
#message("PROTO SRC DIR: ${PROTO_SRC_DIR}")
#message("PROTOs: ${PROTOS}")
#file(MAKE_DIRECTORY ${PROTO_SRC_DIR})
#include_directories(${PROTO_SRC_DIR})
#
#protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_SRC_DIR} ${PROTOS})
#
