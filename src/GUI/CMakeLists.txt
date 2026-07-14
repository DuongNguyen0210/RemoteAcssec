# Bật các công cụ biên dịch tự động của Qt cho UI
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Khai báo các file source, header và UI cho module GUI
set(GUI_SOURCES dummy.cpp
    # Thêm file vào đây sau, ví dụ:
    # MainWindow.cpp MainWindow.h MainWindow.ui
)

add_library(GUI STATIC ${GUI_SOURCES})

# GUI chắc chắn cần Widgets và Gui module của Qt
target_link_libraries(GUI PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
)

target_include_directories(GUI PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})