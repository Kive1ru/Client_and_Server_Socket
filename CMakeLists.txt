cmake_minimum_required(VERSION 3.22)
project(Assignment_5 C)

set(CMAKE_C_STANDARD 11)

add_executable(Assignment_5
        "Client Domain/client.c"
        "Client Domain/Md5.c"
        "Server Domain/Md5.c"
        "Server Domain/server.c")
