cmake_minimum_required(VERSION 3.10)
project(rk3588_screen_detect LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)

# Sysroot lib paths
set(RKNN_LIB_DIR  ${CMAKE_SYSROOT}/usr/lib)
set(RGA_LIB_DIR   ${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu)
set(RKNN_INC_DIR  ${CMAKE_SYSROOT}/usr/include)
set(RGA_INC_DIR   ${CMAKE_SYSROOT}/usr/include/rga)

add_subdirectory(algo)
