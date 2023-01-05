cmake_minimum_required(VERSION 3.15)
project(tun_lab C)

set(CMAKE_C_STANDARD 11)

include_directories(${CMAKE_SOURCE_DIR}/include)
link_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(main src/wintun-demo/main.c)
add_executable(example src/wintun-demo/example.c)
add_executable(start src/wintun-demo/start.c)
add_executable(demo src/emu-demo/tap-win32.c include/tap-win32.h src/emu-demo/vm.c src/emu-demo/emu.c include/emu.h src/emu-demo/tap-my.c include/tap-my.h)

target_link_libraries(start ntdll Iphlpapi Ws2_32)
target_link_libraries(main ntdll Iphlpapi Ws2_32)
target_link_libraries(example ntdll Iphlpapi Ws2_32)