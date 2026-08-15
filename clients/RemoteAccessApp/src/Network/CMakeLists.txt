# Khai báo các file source cho module Network
set(NETWORK_SOURCES dummy.cpp
    # Thêm file vào đây sau, ví dụ:
    # TcpConnection.cpp TcpConnection.h
    # ProtocolDefs.h
)

add_library(Network STATIC ${NETWORK_SOURCES})

# Tìm package Qt Network (bắt buộc cho module này)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)

# Link module Network với thư viện Qt Network
target_link_libraries(Network PRIVATE Qt${QT_VERSION_MAJOR}::Network)

# Cho phép các target khác include header của Network dễ dàng
target_include_directories(Network PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})