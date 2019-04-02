find_package(Catch2 REQUIRED)

add_executable(test test.cpp)

target_include_directories(test
    PUBLIC
        ./
        ../
        # ${BOOST_INCLUDE_DIRS}
        # ${PROTOBUF_INCLUDE_DIRS}
        # ${CRYPTOPP_INCLUDE_DIR}
        # "/usr/include/cryptopp"
)

target_link_libraries(test
    PUBLIC
        libpeerpaste
)
