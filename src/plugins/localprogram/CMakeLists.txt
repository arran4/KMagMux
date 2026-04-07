project(LocalProgramPlugin)

add_library(localprogram MODULE LocalProgramConnector.cpp)
target_link_libraries(localprogram PRIVATE KMagMuxCore Qt6::Core)

# Ensure the plugin is built to the KMagMux plugins folder
set_target_properties(localprogram PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

install(TARGETS localprogram DESTINATION ${CMAKE_INSTALL_LIBDIR}/kmagmux/plugins)
