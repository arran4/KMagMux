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
