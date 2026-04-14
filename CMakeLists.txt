cmake_minimum_required(VERSION 3.16)
project(JDReader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.5.3/gcc_64")

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Widgets WebEngineWidgets WebChannel Sql Svg)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/sidebar/BookshelfPanel.h
    src/sidebar/BookshelfPanel.cpp
    src/reader/ReaderView.h
    src/reader/ReaderView.cpp
    src/reader/WebBridge.h
    src/reader/WebBridge.cpp
    src/storage/Database.h
    src/storage/Database.cpp
    src/storage/BookmarkManager.h
    src/storage/BookmarkManager.cpp
)

qt_add_executable(JDReader ${SOURCES} assets.qrc)

target_link_libraries(JDReader PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
    Qt6::Sql
    Qt6::Svg
)

target_include_directories(JDReader PRIVATE src)
