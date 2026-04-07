include(CTest)

# LocalProgram test
set(CMAKE_AUTOMOC ON)
add_executable(TestLocalProgramConnector TestLocalProgramConnector.cpp ../../../src/plugins/localprogram/LocalProgramConnector.cpp)
target_include_directories(TestLocalProgramConnector PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(TestLocalProgramConnector PRIVATE Qt6::Core Qt6::Test KMagMuxCore)
add_test(NAME TestLocalProgramConnector COMMAND TestLocalProgramConnector)
