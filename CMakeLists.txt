cmake_minimum_required(VERSION 3.16)
project(northstar_travel_planner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(travel_service src/service.cpp)
target_include_directories(travel_service PUBLIC include)

add_executable(server src/main.cpp)
target_link_libraries(server PRIVATE travel_service)

if(WIN32)
  target_link_libraries(server PRIVATE ws2_32)
endif()

add_executable(challenge_tests tests/test_runner.cpp)
target_link_libraries(challenge_tests PRIVATE travel_service)

enable_testing()
add_test(NAME challenge_tests COMMAND challenge_tests)
