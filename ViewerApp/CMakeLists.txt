cmake_minimum_required(VERSION 3.19)
project(ViewerApp LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network LinguistTools)

qt_standard_project_setup()

qt_add_executable(ViewerApp
    WIN32 MACOSX_BUNDLE
    main.cpp
    src/viewer/network/ViewerClient.cpp
    src/viewer/network/ViewerClient.h
    src/viewer/ui/ViewerWindow.cpp
    src/viewer/ui/ViewerWindow.h
)

qt_add_translations(
    TARGETS ViewerApp
    TS_FILES ViewerApp_vi_VN.ts
)

target_link_libraries(ViewerApp
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
)

target_include_directories(ViewerApp
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/viewer/network
        ${CMAKE_CURRENT_SOURCE_DIR}/src/viewer/ui
)

include(GNUInstallDirs)

install(TARGETS ViewerApp
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ViewerApp
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
