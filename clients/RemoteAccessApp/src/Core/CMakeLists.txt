# Tự động quét toàn bộ .cpp và .h trong thư mục Core và các thư mục con
# CONFIGURE_DEPENDS: CMake tự chạy lại configure khi có file mới
file(GLOB_RECURSE CORE_SOURCES
    CONFIGURE_DEPENDS
    "*.cpp"
    "*.h"
)

# Nếu chưa có file nào, thêm dummy.cpp để tránh lỗi "no source files"
if(NOT CORE_SOURCES)
    list(APPEND CORE_SOURCES dummy.cpp)
endif()

add_library(Core STATIC ${CORE_SOURCES})

# Link với Qt (nếu Core có dùng QString, QObject...)
target_link_libraries(Core PRIVATE Qt${QT_VERSION_MAJOR}::Core)

# Cho phép các module khác (như GUI) include file .h của Core mà không cần gõ đường dẫn dài
target_include_directories(Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})