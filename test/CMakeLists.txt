cmake_minimum_required(VERSION 3.2)

project(libeventcpptest)

function(add_libevent_testcase TESTCASENAME TESTCASESOURCES)
    add_executable(${TESTCASENAME} ${TESTCASESOURCES})
    target_include_directories(${TESTCASENAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/event
        ${CMAKE_SOURCE_DIR}/src/http
        ${CMAKE_SOURCE_DIR}/src/thread
        ${CMAKE_SOURCE_DIR}/src/util)
#    target_link_directories(${TESTCASENAME} ${CMAKE_BUILD_DIR})
    target_link_libraries(${TESTCASENAME} PRIVATE libeventcpp_s)
endfunction(add_libevent_testcase)

#Benchmarks
add_libevent_testcase(bench benchmark/bench.cc)
add_libevent_testcase(regress benchmark/regress.cc)
add_libevent_testcase(regress_http_client benchmark/regress_http_client.cc)
add_libevent_testcase(regress_http_server benchmark/regress_http_server.cc)
add_libevent_testcase(regress_thread benchmark/regress_thread.cc)
add_libevent_testcase(server benchmark/server.cc)
add_libevent_testcase(test-eof benchmark/test-eof.cc)
add_libevent_testcase(test-time benchmark/test-time.cc)
add_libevent_testcase(test-weof benchmark/test-weof.cc)

#Tests
add_libevent_testcase(pooltest sample/pool-test.cc)
add_libevent_testcase(rweventtest sample/rw-event-test.cc)
add_libevent_testcase(signaltest sample/signal-test.cc)
add_libevent_testcase(timetest sample/time-test.cc)

#Webbench
#add_libevent_testcase(socket.c webbench/socket.c)
#add_libevent_testcase(webbench.cc webbench/webbench.cc)

