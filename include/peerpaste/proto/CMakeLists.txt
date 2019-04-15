include(FindProtobuf)
find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER message.proto)
add_library(proto ${PROTO_HEADER} ${PROTO_SRC})
target_include_directories(proto
    PRIVATE
        ${PROTOBUF_INCLUDE_DIR}
    )

