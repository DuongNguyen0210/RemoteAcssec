cmake_minimum_required(VERSION 3.16)
project(RemoteAccessApp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Svg Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Svg Core Gui Widgets)

add_subdirectory(src/Utils)
add_subdirectory(src/Core)
add_subdirectory(src/Network)
add_subdirectory(src/GUI)

add_executable(${PROJECT_NAME}
    src/main.cpp
    project.qrc
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Svg
    Utils
    Core
    Network
    GUI
)
