cmake_minimum_required(VERSION 3.14)
project(ProcessSync CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

add_executable(receiver receiver.cpp)
add_executable(sender sender.cpp)

add_executable(message_queue_test message_queue_test.cpp)
target_link_libraries(message_queue_test GTest::gtest_main)

enable_testing()
add_test(NAME MessageQueueTest COMMAND message_queue_test)