cmake_minimum_required(VERSION 3.26)
project(pnglitcher C)

set(CMAKE_C_STANDARD 99)

add_executable(pnglitcher main.c
        helper.c
        png.c
        decode.c
        encode.c)
target_link_libraries(pnglitcher z)
