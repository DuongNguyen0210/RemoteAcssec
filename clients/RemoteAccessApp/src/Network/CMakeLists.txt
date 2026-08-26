# Khai báo các file source cho module Network
set(NETWORK_SOURCES
    # Phase 1A.1 – wire-protocol foundation (header-only, listed for IDE indexing)
    Protocol/ProtocolConstants.h
    # Phase 1A.2A – wire-protocol header model (header-only, listed for IDE indexing)
    Protocol/ProtocolHeader.h
    # Phase 1A.2B – header serialization
    Protocol/ProtocolSerializer.h
    Protocol/ProtocolSerializer.cpp
)

add_library(Network STATIC ${NETWORK_SOURCES}
    relayclient.h relayclient.cpp
    Authservice.h Authservice.cpp
)

# Tìm package Qt Network (bắt buộc cho module này)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Network)

# Link module Network với thư viện Qt Network
target_link_libraries(Network PUBLIC Qt${QT_VERSION_MAJOR}::Network)

# Cho phép các target khác include header của Network dễ dàng
# Bao gồm cả thư mục con Protocol (ví dụ: #include "Protocol/ProtocolConstants.h")
target_include_directories(Network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Protocol
)
