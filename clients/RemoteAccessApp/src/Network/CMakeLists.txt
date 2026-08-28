file(GLOB_RECURSE NETWORK_SOURCES
    CONFIGURE_DEPENDS
    "*.cpp"
    "*.h"
)

add_library(Network STATIC ${NETWORK_SOURCES})

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)

target_link_libraries(Network PUBLIC Qt${QT_VERSION_MAJOR}::Network)

target_include_directories(Network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
)
