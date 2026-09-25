set(CMAKE_AUTOMOC ON)

file(GLOB_RECURSE CONTROLLERS_SOURCES
    CONFIGURE_DEPENDS
    "*.cpp"
    "*.h"
)

add_library(Controllers STATIC ${CONTROLLERS_SOURCES})

target_link_libraries(Controllers PUBLIC
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Core
    GUI
    Domain
    Network
    Streaming
    Utils
)

target_include_directories(Controllers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
