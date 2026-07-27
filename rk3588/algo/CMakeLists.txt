project(rk3588_algo LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)

# RKNN + RGA include dirs
set(RKNN_INC_DIR  ${CMAKE_SYSROOT}/usr/include)
set(RGA_INC_DIR   ${CMAKE_SYSROOT}/usr/include/rga)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${RKNN_INC_DIR}
    ${RGA_INC_DIR}
)

# === libscreen_detect_rknn.so ===
add_library(screen_detect_rknn SHARED
    screen_detect_wrapper.cpp
    common/json_parser.cpp
    common/postprocess.cpp
)

set_target_properties(screen_detect_rknn PROPERTIES
    PREFIX "lib"
    SUFFIX ".so"
)

# Link: rknnrt (NPU) + rga (硬件resize) + mosquitto (MQTT) + pthread + dl + jpeg + stdc++
target_link_directories(screen_detect_rknn PRIVATE
    ${CMAKE_SYSROOT}/usr/lib
    ${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu
)

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
