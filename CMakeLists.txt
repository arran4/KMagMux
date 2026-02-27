cmake_minimum_required(VERSION 3.16)

project(KMagMux VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(KMagMux
    src/main.cpp
    src/core/Item.cpp
    src/core/StorageManager.cpp
    src/core/ItemModel.cpp
    src/ui/MainWindow.cpp
)

target_link_libraries(KMagMux PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

install(TARGETS KMagMux DESTINATION bin)
install(FILES KMagMux.desktop DESTINATION share/applications)
