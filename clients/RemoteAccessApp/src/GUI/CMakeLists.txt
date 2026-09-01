set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

file(GLOB_RECURSE GUI_SOURCES
    CONFIGURE_DEPENDS
    "*.cpp"
    "*.h"
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_library(GUI STATIC ${GUI_SOURCES})

target_link_libraries(GUI PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Network
    Core
)

target_include_directories(GUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
