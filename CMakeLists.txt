cmake_minimum_required(VERSION 3.15)
project(tun_lab C)

set(CMAKE_C_STANDARD 11)

include_directories(${CMAKE_SOURCE_DIR}/include)
link_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(main src/main.c)
add_executable(example src/example.c)

target_link_libraries(main ntdll Iphlpapi Ws2_32)
target_link_libraries(example ntdll Iphlpapi Ws2_32)