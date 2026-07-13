cmake_minimum_required(VERSION 3.19)
project(HostApp LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network LinguistTools)

qt_standard_project_setup()

qt_add_executable(HostApp
    WIN32 MACOSX_BUNDLE
    main.cpp
    hostwindow.cpp
    hostwindow.h
)

qt_add_translations(
    TARGETS HostApp
    TS_FILES HostApp_vi_VN.ts
)

target_link_libraries(HostApp
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
)

include(GNUInstallDirs)

install(TARGETS HostApp
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET HostApp
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
