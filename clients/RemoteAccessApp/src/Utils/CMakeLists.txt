file(GLOB_RECURSE UTILS_SOURCES
    CONFIGURE_DEPENDS
    "*.cpp"
    "*.h"
)

if(NOT UTILS_SOURCES)
    list(APPEND UTILS_SOURCES Dummy.cpp)
endif()

add_library(Utils STATIC ${UTILS_SOURCES})

target_link_libraries(Utils PRIVATE Qt${QT_VERSION_MAJOR}::Core)

target_include_directories(Utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})