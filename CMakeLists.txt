cmake_minimum_required(VERSION 3.16)

project(KMagMux VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent)
find_package(Qt5Keychain REQUIRED)
find_package(KF5XmlGui REQUIRED)

include(GNUInstallDirs)

# Create Core Library
add_library(KMagMuxCore SHARED
    src/core/Item.cpp
    src/core/StorageManager.cpp
    src/core/Engine.cpp
    src/core/ItemModel.cpp
    src/core/ItemParser.cpp
    src/core/SecureStorage.cpp
)
target_link_libraries(KMagMuxCore PUBLIC Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Network Qt5::Concurrent qt5keychain)
target_include_directories(KMagMuxCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(RELATIVE_PATH REL_PLUGIN_DIR
     "${CMAKE_INSTALL_FULL_BINDIR}"
     "${CMAKE_INSTALL_FULL_LIBDIR}/kmagmux/plugins")
target_compile_definitions(KMagMuxCore PRIVATE KMAGMUX_REL_PLUGIN_DIR=\"${REL_PLUGIN_DIR}\")

set_target_properties(KMagMuxCore PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_executable(KMagMux
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/AddItemDialog.cpp
    src/ui/ProcessItemDialog.cpp
    src/ui/PreferencesDialog.cpp
    src/ui/LinkExtractorDialog.cpp
    src/ui/ItemFilterProxyModel.cpp
    resources/resources.qrc
)

target_link_libraries(KMagMux PRIVATE KMagMuxCore Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Concurrent KF5::XmlGui)

add_subdirectory(src/plugins/qbittorrent)
add_subdirectory(src/plugins/torbox)
add_subdirectory(src/plugins/putio)
add_subdirectory(src/plugins/premiumize)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

install(TARGETS KMagMux KMagMuxCore DESTINATION bin)
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

# Install KXmlGui UI definition
install(FILES src/ui/kmagmuxui.rc DESTINATION share/kxmlgui5/kmagmux)
