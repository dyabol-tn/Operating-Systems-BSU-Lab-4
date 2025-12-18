cmake_minimum_required(VERSION 3.14)
project(ProcessSync CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MDd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MD")
    
    add_definitions(-D_WIN32_WINNT=0x0601)
    set(WIN32_LIBS kernel32.lib user32.lib)
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)

if(MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "Use shared CRT for gtest" FORCE)
endif()

set(BUILD_GMOCK OFF CACHE BOOL "Disable GMock")
set(INSTALL_GTEST OFF CACHE BOOL "Disable GTest installation")

FetchContent_MakeAvailable(googletest)

# git clone https://github.com/google/googletest.git

add_executable(receiver receiver.cpp)
add_executable(sender sender.cpp)

if(WIN32)
    target_link_libraries(receiver ${WIN32_LIBS})
    target_link_libraries(sender ${WIN32_LIBS})
endif()

add_executable(message_queue_test message_queue_test.cpp)
if(TARGET gtest_main)
    target_link_libraries(message_queue_test gtest_main gtest)
elseif(TARGET GTest::gtest_main)
    target_link_libraries(message_queue_test GTest::gtest_main GTest::gtest)
endif()

if(WIN32)
    target_link_libraries(message_queue_test ${WIN32_LIBS})
endif()

enable_testing()
add_test(NAME MessageQueueTest COMMAND message_queue_test)

set_target_properties(receiver sender message_queue_test
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS message_queue_test
    COMMENT "Running tests..."
)