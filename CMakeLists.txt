cmake_minimum_required(VERSION 3.16)

project(KMagMux VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

add_executable(KMagMux
    src/main.cpp
    src/core/Item.cpp
    src/core/StorageManager.cpp
    src/core/Engine.cpp
    src/core/ItemModel.cpp
    src/core/Connector.cpp
    src/core/QBittorrentConnector.cpp
    src/ui/MainWindow.cpp
    src/ui/AddItemDialog.cpp
    resources/resources.qrc
)

target_link_libraries(KMagMux PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

install(TARGETS KMagMux DESTINATION bin)
install(FILES KMagMux.desktop DESTINATION share/applications)

# Install icons
install(FILES resources/icons/kmagmux.svg DESTINATION share/icons/hicolor/scalable/apps)
install(FILES resources/icons/kmagmux-16.png DESTINATION share/icons/hicolor/16x16/apps RENAME kmagmux.png)
install(FILES resources/icons/kmagmux-32.png DESTINATION share/icons/hicolor/32x32/apps RENAME kmagmux.png)
install(FILES resources/icons/kmagmux-48.png DESTINATION share/icons/hicolor/48x48/apps RENAME kmagmux.png)
install(FILES resources/icons/kmagmux-64.png DESTINATION share/icons/hicolor/64x64/apps RENAME kmagmux.png)
install(FILES resources/icons/kmagmux-128.png DESTINATION share/icons/hicolor/128x128/apps RENAME kmagmux.png)
install(FILES resources/icons/kmagmux-256.png DESTINATION share/icons/hicolor/256x256/apps RENAME kmagmux.png)
install(FILES resources/icons/kmagmux-512.png DESTINATION share/icons/hicolor/512x512/apps RENAME kmagmux.png)
