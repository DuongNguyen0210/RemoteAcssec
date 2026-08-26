set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(GUI_SOURCES
    mainwindow.cpp
    mainwindow.h

    loginwindow.cpp
    loginwindow.h

    Components/sidebarwidget.cpp
    Components/sidebarwidget.h
    Components/topbarwidget.cpp
    Components/topbarwidget.h
    Components/devicecardwidget.cpp
    Components/devicecardwidget.h

    Pages/devicespage.cpp
    Pages/devicespage.h
    Pages/sessionspage.cpp
    Pages/sessionspage.h
    Pages/settingspage.cpp
    Pages/settingspage.h
    Pages/logspage.cpp
    Pages/logspage.h

    Layouts/flowlayout.cpp
    Layouts/flowlayout.h
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_library(GUI STATIC ${GUI_SOURCES})

target_link_libraries(GUI PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Network
)
target_link_libraries(GUI PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

target_include_directories(GUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
