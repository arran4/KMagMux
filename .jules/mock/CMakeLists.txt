cmake_minimum_required(VERSION 3.16)

project(KMagMuxMock VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Concurrent)
find_package(Qt6Keychain REQUIRED)
find_package(KF6XmlGui REQUIRED)

# The mock app needs KMagMuxCore but also the UI files. We can compile them in
# directly or link against the object files if possible. Alternatively, we can just
# add the UI source files here.

add_executable(KMagMuxMock
    mock_app.cpp
    ../../src/ui/MainWindow.cpp
    ../../src/ui/AddItemDialog.cpp
    ../../src/ui/ProcessItemDialog.cpp
    ../../src/ui/PreferencesDialog.cpp
    ../../src/ui/LinkExtractorDialog.cpp
    ../../src/ui/ItemFilterProxyModel.cpp
    ../../src/ui/ApiExplorerDialog.cpp
    ../../src/ui/TorrentInfoDialog.cpp
    ../../src/ui/MaxWidthDelegate.h
    ../../resources/resources.qrc
)

target_link_libraries(KMagMuxMock PRIVATE KMagMuxCore Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent KF6::XmlGui)
target_include_directories(KMagMuxMock PRIVATE ../../src)
