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

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(Controllers PRIVATE X11::X11 ${CMAKE_DL_LIBS})
elseif(WIN32)
    target_link_libraries(Controllers PRIVATE user32)
endif()

target_include_directories(Controllers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
