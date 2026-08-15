# Khai báo tất cả các file .cpp, .h sẽ có trong Core ở đây
set(CORE_SOURCES dummy.cpp
    # Thêm file ở đây sau, ví dụ: Session/SessionManager.cpp
)

add_library(Core STATIC ${CORE_SOURCES})

# Link với Qt (nếu Core có dùng QString, QObject...)
target_link_libraries(Core PRIVATE Qt${QT_VERSION_MAJOR}::Core)

# Cho phép các module khác (như GUI) include file .h của Core mà không cần gõ đường dẫn dài
target_include_directories(Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})