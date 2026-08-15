# Khai báo các file source cho module Utils
set(UTILS_SOURCES dummy.cpp
    # Thêm file vào đây sau, ví dụ:
    # Logger.cpp Logger.h
    # PlatformUtils.h
)

add_library(Utils STATIC ${UTILS_SOURCES})

# Utils thường chỉ cần xài các class cơ bản như QString, QByteArray, QSettings
target_link_libraries(Utils PRIVATE Qt${QT_VERSION_MAJOR}::Core)

target_include_directories(Utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})