cmake_minimum_required(VERSION 3.16)
project(RemoteAccessApp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Svg Core Gui Widgets Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Svg Core Gui Widgets Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS LinguistTools)

set(translation_source "${CMAKE_CURRENT_SOURCE_DIR}/Resources/translations/qt_vi.ts")
set(translation_catalog "${CMAKE_CURRENT_BINARY_DIR}/qt_vi.qm")
set(translation_qrc "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc")
set(translation_resource "${CMAKE_CURRENT_BINARY_DIR}/qrc_translations.cpp")

file(GENERATE OUTPUT "${translation_qrc}" CONTENT
    "<RCC><qresource prefix=\"/translations\"><file>qt_vi.qm</file></qresource></RCC>")

add_custom_command(
    OUTPUT "${translation_catalog}"
    COMMAND Qt${QT_VERSION_MAJOR}::lrelease "${translation_source}" -qm "${translation_catalog}"
    DEPENDS "${translation_source}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${translation_resource}"
    COMMAND Qt${QT_VERSION_MAJOR}::rcc --name translations --output "${translation_resource}" "${translation_qrc}"
    DEPENDS "${translation_catalog}" "${translation_qrc}"
    VERBATIM
)

add_library(AppTranslations OBJECT "${translation_resource}")
target_link_libraries(AppTranslations PRIVATE Qt${QT_VERSION_MAJOR}::Core)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_subdirectory(src/Utils)
add_subdirectory(src/Domain)
add_subdirectory(src/Streaming)
add_subdirectory(src/Network)
add_subdirectory(src/GUI)
add_subdirectory(src/Controllers)

add_executable(${PROJECT_NAME}
    src/Main.cpp
    project.qrc
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    AppTranslations
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Svg
    Utils
    Domain
    Streaming
    Network
    GUI
    Controllers
)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(AccountDeviceFlowTest tests/AccountDeviceFlowTest.cpp project.qrc)
    target_link_libraries(AccountDeviceFlowTest PRIVATE
        AppTranslations Controllers GUI Streaming Network Domain Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
    add_test(NAME account_device_flow COMMAND AccountDeviceFlowTest)
    set_tests_properties(account_device_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(ProtocolContractTest
        tests/ProtocolContractTest.cpp
    )

    target_link_libraries(ProtocolContractTest PRIVATE
        Network
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Core
    )

    add_test(
        NAME protocol_contract
        COMMAND ProtocolContractTest
    )

    add_executable(AdminSessionCorrelationTest
        tests/AdminSessionCorrelationTest.cpp
    )

    target_link_libraries(AdminSessionCorrelationTest PRIVATE
        Network
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Network
    )

    add_test(
        NAME admin_session_correlation
        COMMAND AdminSessionCorrelationTest
    )

    add_executable(ScreenFrameReassemblerTest
        tests/ScreenFrameReassemblerTest.cpp
    )

    target_link_libraries(ScreenFrameReassemblerTest PRIVATE
        Network
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Core
    )

    add_test(
        NAME screen_frame_reassembler
        COMMAND ScreenFrameReassemblerTest
    )

    add_executable(SseClientTest tests/SseClientTest.cpp)
    target_link_libraries(SseClientTest PRIVATE Network Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Network)
    add_test(NAME sse_client COMMAND SseClientTest)
endif()

if(BUILD_TESTING)
    add_executable(RelaySessionTest tests/RelaySessionTest.cpp)
    target_link_libraries(RelaySessionTest PRIVATE Network Qt${QT_VERSION_MAJOR}::Test)
    add_test(NAME relay_sessions COMMAND RelaySessionTest)
endif()

if(BUILD_TESTING)
    add_executable(AdminScreenViewerTest tests/AdminScreenViewerTest.cpp)
    target_link_libraries(AdminScreenViewerTest PRIVATE Controllers Network GUI Streaming Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
    add_test(NAME admin_screen_viewer COMMAND AdminScreenViewerTest)
    set_tests_properties(admin_screen_viewer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
