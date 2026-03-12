cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

# Need to build a mock plugin to test loading
add_library(mockplugin MODULE MockPlugin.cpp MockPlugin.h)
target_link_libraries(mockplugin PRIVATE KMagMuxCore Qt6::Core)
set_target_properties(mockplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/plugins"
)

add_executable(TestEngine TestEngine.cpp)
target_link_libraries(TestEngine PRIVATE KMagMuxCore Qt6::Core Qt6::Test)
target_include_directories(TestEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
add_dependencies(TestEngine mockplugin)

# Pass the expected mock plugin filename to the test based on platform
if(WIN32)
    target_compile_definitions(TestEngine PRIVATE MOCK_PLUGIN_FILENAME="mockplugin.dll")
elseif(APPLE)
    target_compile_definitions(TestEngine PRIVATE MOCK_PLUGIN_FILENAME="libmockplugin.dylib")
else()
    target_compile_definitions(TestEngine PRIVATE MOCK_PLUGIN_FILENAME="libmockplugin.so")
endif()

add_test(NAME TestEngine COMMAND TestEngine)

add_executable(TestStorageManager TestStorageManager.cpp)
target_link_libraries(TestStorageManager PRIVATE KMagMuxCore Qt6::Core Qt6::Test)
target_include_directories(TestStorageManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
add_test(NAME TestStorageManager COMMAND TestStorageManager)

add_executable(TestBencodeParser TestBencodeParser.cpp)
target_link_libraries(TestBencodeParser PRIVATE KMagMuxCore Qt6::Core Qt6::Test)
target_include_directories(TestBencodeParser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
add_test(NAME TestBencodeParser COMMAND TestBencodeParser)

add_executable(TestTorrentParser TestTorrentParser.cpp)
target_link_libraries(TestTorrentParser PRIVATE KMagMuxCore Qt6::Core Qt6::Test)
target_include_directories(TestTorrentParser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
add_test(NAME TestTorrentParser COMMAND TestTorrentParser)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE KMagMuxCore Qt6::Core Qt6::Test)
target_include_directories(benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
