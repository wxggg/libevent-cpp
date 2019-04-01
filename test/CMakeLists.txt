cmake_minimum_required(VERSION 3.2)

project(libeventcpptest)

set(INCLUDE_DIRECTORIES
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/event
    ${CMAKE_SOURCE_DIR}/src/http
    ${CMAKE_SOURCE_DIR}/src/thread
    ${CMAKE_SOURCE_DIR}/src/util
)

#pooltest
add_executable(pooltest sample/pool-test.cc)
target_include_directories(pooltest PRIVATE ${INCLUDE_DIRECTORIES})
target_link_directories(pooltest PRIVATE ${CMAKE_BUILD_DIR})
target_link_libraries(pooltest PRIVATE libeventcpp_s)

#rw event test
add_executable(rweventtest sample/rw-event-test.cc)
target_include_directories(rweventtest PRIVATE ${INCLUDE_DIRECTORIES})
target_link_directories(rweventtest PRIVATE ${CMAKE_BUILD_DIR})
target_link_libraries(rweventtest PRIVATE libeventcpp_s)

#signal test
add_executable(signaltest sample/signal-test.cc)
target_include_directories(signaltest PRIVATE ${INCLUDE_DIRECTORIES})
target_link_directories(signaltest PRIVATE ${CMAKE_BUILD_DIR})
target_link_libraries(signaltest PRIVATE libeventcpp_s)

#time test
add_executable(timetest sample/time-test.cc)
target_include_directories(timetest PRIVATE ${INCLUDE_DIRECTORIES})
target_link_directories(timetest PRIVATE ${CMAKE_BUILD_DIR})
target_link_libraries(timetest PRIVATE libeventcpp_s)

