cmake_minimum_required(VERSION 3.18)

project(libera-protocol VERSION 0.1.0 LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(LIBERA_PROTOCOL_DEFAULT_BUILD_TESTS ON)
else()
    set(LIBERA_PROTOCOL_DEFAULT_BUILD_TESTS OFF)
endif()
option(LIBERA_PROTOCOL_BUILD_TESTS
    "Build libera-protocol tests"
    ${LIBERA_PROTOCOL_DEFAULT_BUILD_TESTS})

add_library(libera-protocol-core
    src/Codec.cpp
    src/Receiver.cpp
    src/Sender.cpp
)
add_library(libera-protocol::core ALIAS libera-protocol-core)

target_compile_features(libera-protocol-core PUBLIC cxx_std_17)
target_include_directories(libera-protocol-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(libera-protocol-core PRIVATE /W4 /permissive-)
else()
    target_compile_options(libera-protocol-core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(LIBERA_PROTOCOL_BUILD_TESTS)
    enable_testing()

    add_executable(test_codec tests/test_codec.cpp)
    target_link_libraries(test_codec PRIVATE libera-protocol-core)
    add_test(NAME test_codec COMMAND test_codec)
endif()
