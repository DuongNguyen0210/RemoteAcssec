set(CMAKE_AUTOMOC ON)

file(GLOB_RECURSE NETWORK_SOURCES
    CONFIGURE_DEPENDS
    "*.cpp"
    "*.h"
)

add_library(Network STATIC ${NETWORK_SOURCES})

target_link_libraries(Network PUBLIC
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Core
)

target_include_directories(Network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Http
    ${CMAKE_CURRENT_SOURCE_DIR}/Relay
    ${CMAKE_CURRENT_SOURCE_DIR}/Protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
