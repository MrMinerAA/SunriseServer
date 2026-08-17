cmake_minimum_required(VERSION 3.16)
project(sunriseserver)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

file(GLOB_RECURSE project_source CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/SunriseServer/src/*.cpp ${CMAKE_SOURCE_DIR}/SunriseServer/src/*.h)

add_executable(sunrise_server ${project_source})
target_link_libraries(sunrise_server PRIVATE ws2_32)
target_link_options(sunrise_server PRIVATE
    -static
    -static-libgcc
    -static-libstdc++
)
target_include_directories(sunrise_server PRIVATE
    ${CMAKE_SOURCE_DIR}/SunriseServer/src
)
