cmake_minimum_required(VERSION 3.16)
project(RemoteControlSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Network)

# Enable CTest from this top-level configure root
enable_testing()

# Add subdirectory for the main GUI client
add_subdirectory(RemoteAccessApp)
