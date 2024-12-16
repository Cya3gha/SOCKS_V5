# CMakeList.txt : CMake project for challenge_socks, include source and define
# project specific logic here.
#

cmake_minimum_required(VERSION 3.10)
project(Socks_v5)

# Add source to this project's executable.
add_executable(challenge_socks challenge_socks.cpp)


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET challenge_socks PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.

set(ASIO_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}\\..\\asio-1.30.2\\include")
include_directories(${ASIO_INCLUDE_DIR})

target_link_libraries(challenge_socks PRIVATE Ws2_32)
target_compile_definitions(challenge_socks PRIVATE _WIN32_WINNT=0x0601)

