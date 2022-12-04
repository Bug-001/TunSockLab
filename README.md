# Tun Sock Lab

This lab is based on [WinTun](https://git.zx2c4.com/wintun/about/).

The `main.exe` executable will install TUN driver on Windows, create an TUN adapter and send TCP connection request via it. After that, an RST packet is sent to close the connection.

## Build & run

`include/common.h` defines some constant, such as:
- TCP Server runs on 172.26.50.199:8080
- TUN Client runs on 10.0.0.1:16888

which should be changed as you need.

It's a CMake project. Use CLion, or immediately build in Windows CMD:
```
mkdir build && cd build
cmake ..
cmake --build .
# Find main.exe executable and run it as administrator
# If error code 0x0000007E is returned, please copy main.exe to ${CMAKE_SOURCE_DIR}/lib/
```
