project(QBittorrentTests)

find_package(Qt6 REQUIRED COMPONENTS Core Network Test)

add_executable(QBittorrentConnectorTest QBittorrentConnectorTest.cpp)
target_link_libraries(QBittorrentConnectorTest PRIVATE Qt6::Core Qt6::Network Qt6::Test KMagMuxCore)

# Define the source file path of QBittorrentConnector so the test can compile it directly
# Or even better, link against a static/object library of QBittorrentConnector.
# For simplicity, we can just compile QBittorrentConnector.cpp into the test.
target_sources(QBittorrentConnectorTest PRIVATE ../QBittorrentConnector.cpp)
target_include_directories(QBittorrentConnectorTest PRIVATE ../)

add_test(NAME QBittorrentConnectorTest COMMAND QBittorrentConnectorTest)
