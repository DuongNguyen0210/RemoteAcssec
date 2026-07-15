set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(GUI_SOURCES
    mainwindow.cpp
    mainwindow.h

    Components/SidebarWidget.cpp
    Components/SidebarWidget.h
    Components/TopBarWidget.cpp
    Components/TopBarWidget.h
    Components/DeviceCardWidget.cpp
    Components/DeviceCardWidget.h

    Pages/DevicesPage.cpp
    Pages/DevicesPage.h
    Pages/SettingsPage.cpp
    Pages/SettingsPage.h
)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

add_library(GUI STATIC ${GUI_SOURCES})

target_link_libraries(GUI PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
)
target_link_libraries(GUI PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(GUI PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

target_include_directories(GUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
