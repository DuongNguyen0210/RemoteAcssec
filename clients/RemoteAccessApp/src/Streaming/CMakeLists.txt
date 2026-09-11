set(CMAKE_AUTOMOC ON)

file(GLOB_RECURSE STREAMING_SOURCES
    CONFIGURE_DEPENDS
    "*.cpp"
    "*.h"
)

add_library(Streaming STATIC ${STREAMING_SOURCES})

target_link_libraries(Streaming PUBLIC
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Core
    Network
    Domain
)

target_include_directories(Streaming PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
