cmake_minimum_required(VERSION 3.2)

project(libeventcpp)

find_package(Threads REQUIRED)

set(SOURCES
    ${PROJECT_SOURCE_DIR}/src/core/async_logger.cc
    ${PROJECT_SOURCE_DIR}/src/core/buffer.cc
    ${PROJECT_SOURCE_DIR}/src/core/logger.cc
    ${PROJECT_SOURCE_DIR}/src/event/buffer_event.cc
    ${PROJECT_SOURCE_DIR}/src/event/epoll_base.cc
    ${PROJECT_SOURCE_DIR}/src/event/event_base.cc
    ${PROJECT_SOURCE_DIR}/src/event/event.cc
    ${PROJECT_SOURCE_DIR}/src/event/poll_base.cc
    ${PROJECT_SOURCE_DIR}/src/event/select_base.cc
    ${PROJECT_SOURCE_DIR}/src/http/http_client.cc
    ${PROJECT_SOURCE_DIR}/src/http/http_client_connection.cc
    ${PROJECT_SOURCE_DIR}/src/http/http_connection.cc
    ${PROJECT_SOURCE_DIR}/src/http/http_request.cc
    ${PROJECT_SOURCE_DIR}/src/http/http_server.cc
    ${PROJECT_SOURCE_DIR}/src/http/http_server_connection.cc
    ${PROJECT_SOURCE_DIR}/src/http/http_server_thread.cc
    ${PROJECT_SOURCE_DIR}/src/thread/thread_pool.cc
    ${PROJECT_SOURCE_DIR}/src/util/util_linux.cc
    ${PROJECT_SOURCE_DIR}/src/util/util_network.cc
    ${PROJECT_SOURCE_DIR}/src/util/util_string.cc
)

set(INCLUDE_DIRECTORIES
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/src/event
    ${PROJECT_SOURCE_DIR}/src/http
    ${PROJECT_SOURCE_DIR}/src/thread
    ${PROJECT_SOURCE_DIR}/src/util
)


option(BUILD_SHARED_LIBS "Build the shared library" OFF)
option(BUILD_TESTS "Build the tests" OFF)

if (BUILD_SHARED_LIBS)
add_library(libeventcpp SHARED ${SOURCES})
target_compile_features(libeventcpp PUBLIC cxx_std_11)
target_include_directories(libeventcpp PRIVATE ${INCLUDE_DIRECTORIES})
target_link_libraries(libeventcpp PRIVATE Threads::Threads)
endif(BUILD_SHARED_LIBS)

add_library(libeventcpp_s STATIC ${SOURCES})
target_compile_features(libeventcpp_s PUBLIC cxx_std_11)
target_include_directories(libeventcpp_s PRIVATE ${INCLUDE_DIRECTORIES})
target_link_libraries(libeventcpp_s PRIVATE Threads::Threads)

if (BUILD_TESTS)
add_subdirectory(test)
endif(BUILD_TESTS)
