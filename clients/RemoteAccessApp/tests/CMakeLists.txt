find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

add_executable(ProtocolSerializerTests
    Protocol/ProtocolSerializerTests.cpp
)

target_link_libraries(ProtocolSerializerTests PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Network
)

add_test(NAME ProtocolSerializerTests COMMAND ProtocolSerializerTests)
