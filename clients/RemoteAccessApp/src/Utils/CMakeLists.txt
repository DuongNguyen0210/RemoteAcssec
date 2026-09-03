# Tự động quét toàn bộ .cpp và .h trong thư mục Utils và các thư mục con
# CONFIGURE_DEPENDS: CMake tự chạy lại configure khi có file mới
file(GLOB_RECURSE UTILS_SOURCES
    CONFIGURE_DEPENDS
    "*.cpp"
    "*.h"
)

# Nếu chưa có file nào, thêm Dummy.cpp để tránh lỗi "no source files"
if(NOT UTILS_SOURCES)
    list(APPEND UTILS_SOURCES Dummy.cpp)
endif()

add_library(Utils STATIC ${UTILS_SOURCES})

# Utils thường chỉ cần xài các class cơ bản như QString, QByteArray, QSettings
target_link_libraries(Utils PRIVATE Qt${QT_VERSION_MAJOR}::Core)

target_include_directories(Utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})