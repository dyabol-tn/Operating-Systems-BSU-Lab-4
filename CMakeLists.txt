cmake_minimum_required(VERSION 3.14)
project(ProcessSync CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
        add_compile_options(/MDd)
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        add_compile_options(/MD)
    endif()
endif()

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(receiver receiver.cpp)
add_executable(sender sender.cpp)

if(UNIX)
    target_link_libraries(receiver pthread)
    target_link_libraries(sender pthread)
endif()

add_executable(message_queue_test message_queue_test.cpp)
target_link_libraries(message_queue_test 
    GTest::gtest_main 
    GTest::gtest
)
if(UNIX)
    target_link_libraries(message_queue_test pthread)
endif()

enable_testing()
add_test(NAME MessageQueueTest COMMAND message_queue_test)

add_compile_options(-Wall -Wextra -Wpedantic)

add_custom_target(test_all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS message_queue_test
    COMMENT "Running all tests..."
)