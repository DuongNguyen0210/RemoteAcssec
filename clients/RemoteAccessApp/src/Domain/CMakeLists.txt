set(CMAKE_AUTOMOC ON)

file(GLOB_RECURSE DOMAIN_SOURCES
    CONFIGURE_DEPENDS
    "*.cpp"
    "*.h"
)

add_library(Domain STATIC ${DOMAIN_SOURCES})

target_link_libraries(Domain PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
)

target_include_directories(Domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
