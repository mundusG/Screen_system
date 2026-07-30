project(rk3588_algo LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)

# Include dirs — 在 RK3588 设备上原生编译时 /usr/include 下有真实头文件；
# WSL 交叉编译时由 toolchain CMAKE_SYSROOT 重定向到 sysroot
if(DEFINED CMAKE_SYSROOT)
    set(RKNN_INC_DIR ${CMAKE_SYSROOT}/usr/include)
    set(RGA_INC_DIR  ${CMAKE_SYSROOT}/usr/include/rga)
else()
    set(RKNN_INC_DIR /usr/include)
    set(RGA_INC_DIR  /usr/include/rga)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${RKNN_INC_DIR}
    ${RGA_INC_DIR}
)

add_library(screen_detect_rknn SHARED
    screen_detect_wrapper.cpp
    common/json_parser.cpp
    common/postprocess.cpp
)

set_target_properties(screen_detect_rknn PROPERTIES
    PREFIX "lib"
    SUFFIX ".so"
)

# Link dirs
if(DEFINED CMAKE_SYSROOT)
    target_link_directories(screen_detect_rknn PRIVATE
        ${CMAKE_SYSROOT}/usr/lib
        ${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu
    )
else()
    target_link_directories(screen_detect_rknn PRIVATE
        /usr/lib
        /usr/lib/aarch64-linux-gnu
    )
endif()

target_link_libraries(screen_detect_rknn PRIVATE
    rknnrt
    rga
    mosquitto
    pthread
    dl
    jpeg
    stdc++
    curl
)
