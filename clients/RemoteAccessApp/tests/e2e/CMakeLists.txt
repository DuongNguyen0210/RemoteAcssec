cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(RemoteAccessAppE2E VERSION 0.1 LANGUAGES CXX)
    include(CTest)
    enable_testing()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

get_filename_component(APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

if(NOT DEFINED QT_VERSION_MAJOR)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent Test)
endif()
if(NOT TARGET Qt${QT_VERSION_MAJOR}::Test OR NOT TARGET Qt${QT_VERSION_MAJOR}::Concurrent)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets Network Concurrent Test)
endif()

include_directories("${APP_ROOT}/src")

if(NOT TARGET Streaming)
    add_subdirectory("${APP_ROOT}/src/Domain"      "${CMAKE_CURRENT_BINARY_DIR}/app_src/Domain"      EXCLUDE_FROM_ALL)
    add_subdirectory("${APP_ROOT}/src/Utils"       "${CMAKE_CURRENT_BINARY_DIR}/app_src/Utils"       EXCLUDE_FROM_ALL)
    add_subdirectory("${APP_ROOT}/src/Network"     "${CMAKE_CURRENT_BINARY_DIR}/app_src/Network"     EXCLUDE_FROM_ALL)
    add_subdirectory("${APP_ROOT}/src/Streaming"   "${CMAKE_CURRENT_BINARY_DIR}/app_src/Streaming"   EXCLUDE_FROM_ALL)
    add_subdirectory("${APP_ROOT}/src/GUI"         "${CMAKE_CURRENT_BINARY_DIR}/app_src/GUI"         EXCLUDE_FROM_ALL)
    add_subdirectory("${APP_ROOT}/src/Controllers" "${CMAKE_CURRENT_BINARY_DIR}/app_src/Controllers" EXCLUDE_FROM_ALL)
endif()

add_library(E2eTestHarness STATIC
    E2eRelayHarness.cpp
    E2eRelayHarness.h
    E2eFrameOracle.cpp
    E2eFrameOracle.h
)

target_include_directories(E2eTestHarness PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${APP_ROOT}/src"
)

target_link_libraries(E2eTestHarness PUBLIC
    Controllers
    GUI
    Streaming
    Network
    Utils
    Domain
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Test
)

function(add_e2e_qt_test target_name ctest_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} PRIVATE E2eTestHarness)
    add_test(NAME ${ctest_name} COMMAND ${target_name})
    set_tests_properties(${ctest_name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_e2e_qt_test(E2eHarnessSelfTest e2e_harness_self_test E2eHarnessSelfTest.cpp)
