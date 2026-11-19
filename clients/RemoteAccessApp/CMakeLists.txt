cmake_minimum_required(VERSION 3.16)
project(RemoteAccessApp VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Svg Core Gui Widgets Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Svg Core Gui Widgets Network Concurrent)

find_program(PBL4_LRELEASE NAMES lrelease-qt${QT_VERSION_MAJOR} lrelease${QT_VERSION_MAJOR} lrelease
    HINTS "${QT_HOST_PATH}/bin" "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin" "/usr/lib/qt${QT_VERSION_MAJOR}/bin")
if(NOT PBL4_LRELEASE)
    message(FATAL_ERROR "Qt lrelease is required to build translations (install qt6-l10n-tools or Qt Linguist tools).")
endif()
set(PBL4_TRANSLATION "${CMAKE_CURRENT_BINARY_DIR}/qt_vi.qm")
add_custom_command(OUTPUT "${PBL4_TRANSLATION}"
    COMMAND "${PBL4_LRELEASE}" "${CMAKE_CURRENT_SOURCE_DIR}/Resources/translations/qt_vi.ts" -qm "${PBL4_TRANSLATION}"
    DEPENDS Resources/translations/qt_vi.ts VERBATIM)
add_custom_target(Pbl4Translations DEPENDS "${PBL4_TRANSLATION}")
set(PBL4_TRANSLATION_QRC "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc")
file(WRITE "${PBL4_TRANSLATION_QRC}" "<RCC><qresource prefix=\"/translations\"><file alias=\"qt_vi.qm\">${PBL4_TRANSLATION}</file></qresource></RCC>")

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
    "${PBL4_TRANSLATION_QRC}"
)
add_dependencies(${PROJECT_NAME} Pbl4Translations)

target_link_libraries(${PROJECT_NAME} PRIVATE
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
option(ENABLE_X11_INPUT_INTEGRATION "Build native input test for an isolated X11 display" OFF)
option(ENABLE_WINDOWS_INPUT_INTEGRATION "Build native input test for a dedicated Windows VM" OFF)
if(BUILD_TESTING AND ENABLE_WINDOWS_INPUT_INTEGRATION AND WIN32)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(WindowsInputIntegrationTest tests/NativeInputIntegrationTest.cpp)
    target_link_libraries(WindowsInputIntegrationTest PRIVATE Controllers Streaming Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
endif()
if(BUILD_TESTING AND ENABLE_X11_INPUT_INTEGRATION AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(X11InputIntegrationTest tests/NativeInputIntegrationTest.cpp)
    target_link_libraries(X11InputIntegrationTest PRIVATE Controllers Streaming Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
    add_executable(DesktopRelayAcceptance tests/DesktopRelayAcceptance.cpp)
    target_link_libraries(DesktopRelayAcceptance PRIVATE Controllers Streaming Qt${QT_VERSION_MAJOR}::Widgets)
    add_executable(FullUiDesktopAcceptance tests/FullUiDesktopAcceptance.cpp project.qrc "${PBL4_TRANSLATION_QRC}")
    add_dependencies(FullUiDesktopAcceptance Pbl4Translations)
    target_link_libraries(FullUiDesktopAcceptance PRIVATE Controllers Streaming Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
endif()
if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(AccountDeviceFlowTest tests/AccountDeviceFlowTest.cpp project.qrc "${PBL4_TRANSLATION_QRC}")
    add_dependencies(AccountDeviceFlowTest Pbl4Translations)
    target_link_libraries(AccountDeviceFlowTest PRIVATE
        Controllers GUI Streaming Network Domain Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
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
    set_tests_properties(screen_frame_reassembler PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(SseClientTest tests/SseClientTest.cpp)
    target_link_libraries(SseClientTest PRIVATE Network Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Network)
    add_test(NAME sse_client COMMAND SseClientTest)
    add_executable(SingleLoginDesktopAcceptance tests/SingleLoginDesktopAcceptance.cpp project.qrc "${PBL4_TRANSLATION_QRC}")
    add_dependencies(SingleLoginDesktopAcceptance Pbl4Translations)
    target_link_libraries(SingleLoginDesktopAcceptance PRIVATE Controllers Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Widgets)
    add_executable(LoginSessionMonitorTest tests/LoginSessionMonitorTest.cpp)
    target_link_libraries(LoginSessionMonitorTest PRIVATE Network Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Network)
    add_test(NAME login_session_monitor COMMAND LoginSessionMonitorTest)
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

if(BUILD_TESTING)
    add_executable(SecurityTransportTest tests/SecurityTransportTest.cpp)
    target_link_libraries(SecurityTransportTest PRIVATE Network Qt${QT_VERSION_MAJOR}::Test Qt${QT_VERSION_MAJOR}::Network)
    add_test(NAME security_transport COMMAND SecurityTransportTest)
endif()
