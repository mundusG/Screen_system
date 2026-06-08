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

# Source files
set(SOURCES
    main.cpp
    src/MainWindow.cpp
    src/VideoWidget.cpp
    src/StatsPanel.cpp
    src/CameraCapture.cpp
    src/InferenceEngine.cpp
    src/SmoothingFilter.cpp
    src/ConfigManager.cpp
    src/SettingsDialog.cpp
)

set(HEADERS
    src/Types.h
    src/MainWindow.h
    src/VideoWidget.h
    src/StatsPanel.h
    src/CameraCapture.h
    src/InferenceEngine.h
    src/SmoothingFilter.h
    src/ConfigManager.h
    src/SettingsDialog.h
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
