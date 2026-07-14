cmake_minimum_required(VERSION 3.16)
project(RemoteAccessApp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

# Khai báo các thư mục con
add_subdirectory(src/Utils)
add_subdirectory(src/Core)
add_subdirectory(src/Network)
add_subdirectory(src/GUI)

# Tạo file thực thi chính
add_executable(${PROJECT_NAME} src/main.cpp)

# Link các module lại với file thực thi
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Utils
    Core
    Network
    GUI
)