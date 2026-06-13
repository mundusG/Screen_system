cmake_minimum_required(VERSION 3.14)
project(ScreenInferenceSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt5
find_package(Qt5 REQUIRED COMPONENTS Widgets Core Gui)

# Find OpenCV
find_package(OpenCV REQUIRED COMPONENTS core imgproc videoio dnn)

# Find Paho MQTT C++
find_package(PahoMqttCpp REQUIRED)

# Source files
set(SOURCES
    main.cpp
    src/MainWindow.cpp
    src/VideoWidget.cpp
    src/StatsPanel.cpp
    src/SidebarWidget.cpp
    src/BottomControlBar.cpp
    src/AlertPanel.cpp
    src/CameraCapture.cpp
    src/InferenceEngine.cpp
    src/SmoothingFilter.cpp
    src/ConfigManager.cpp
    src/SettingsDialog.cpp
    src/MQTTClient.cpp
    src/InferenceSubscriber.cpp
    src/InferencePublisher.cpp
    src/ServiceLauncher.cpp
)

set(HEADERS
    src/Types.h
    src/Theme.h
    src/MainWindow.h
    src/VideoWidget.h
    src/StatsPanel.h
    src/SidebarWidget.h
    src/BottomControlBar.h
    src/AlertPanel.h
    src/CameraCapture.h
    src/InferenceEngine.h
    src/SmoothingFilter.h
    src/ConfigManager.h
    src/SettingsDialog.h
    src/MQTTClient.h
    src/InferenceSubscriber.h
    src/InferencePublisher.h
    src/ServiceLauncher.h
)

# Qt resources
set(RESOURCES resources.qrc)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${RESOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt5::Widgets
    Qt5::Core
    Qt5::Gui
    ${OpenCV_LIBS}
    PahoMqttCpp::paho-mqttpp3
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

# Runtime path resolution
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SCREEN_SYSTEM_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}"
)

# Copy config file to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/default_config.json
    ${CMAKE_CURRENT_BINARY_DIR}/config/default_config.json
    COPYONLY
)

# Install rules
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY config/ DESTINATION share/${PROJECT_NAME}/config)
install(DIRECTORY models/ DESTINATION share/${PROJECT_NAME}/models)

# ============================================================
# RKNN Inference Node (standalone headless app for edge devices)
# ============================================================
option(BUILD_RKNN_INFERENCE "Build RKNN inference standalone app (requires librknn_api)" OFF)

if(BUILD_RKNN_INFERENCE)
    # Find RKNN runtime library
    find_library(RKNN_API_LIB rknn_api
        HINTS /usr/lib /usr/local/lib /usr/lib/aarch64-linux-gnu
        ENV RKNN_API_PATH
    )

    if(NOT RKNN_API_LIB)
        message(FATAL_ERROR "RKNN API library not found. Set RKNN_API_PATH or install librknn_api.")
    endif()

    # Find rknn_api.h
    find_path(RKNN_API_INCLUDE_DIR rknn_api.h
        HINTS /usr/include /usr/local/include
        ENV RKNN_API_PATH
    )

    set(RKNN_SOURCES
        inference_main.cpp
        src/RKNNInferenceEngine.cpp
        src/CameraCapture.cpp
        src/InferencePublisher.cpp
        src/MQTTClient.cpp
        src/ConfigManager.cpp
    )

    set(RKNN_HEADERS
        src/RKNNInferenceEngine.h
        src/Types.h
        src/CameraCapture.h
        src/InferencePublisher.h
        src/MQTTClient.h
        src/ConfigManager.h
    )

    add_executable(RKNNInferenceNode ${RKNN_SOURCES} ${RKNN_HEADERS})

    target_link_libraries(RKNNInferenceNode PRIVATE
        Qt5::Core
        ${OpenCV_LIBS}
        ${RKNN_API_LIB}
        PahoMqttCpp::paho-mqttpp3
    )

    target_include_directories(RKNNInferenceNode PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
        ${RKNN_API_INCLUDE_DIR}
    )

    target_compile_definitions(RKNNInferenceNode PRIVATE
        SCREEN_SYSTEM_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}"
    )

    install(TARGETS RKNNInferenceNode DESTINATION bin)
    message(STATUS "RKNN Inference Node: ENABLED (lib: ${RKNN_API_LIB})")
else()
    message(STATUS "RKNN Inference Node: DISABLED (use -DBUILD_RKNN_INFERENCE=ON to enable)")
endif()
