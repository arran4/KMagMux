project(TorBoxPlugin)

add_library(torbox MODULE TorBoxConnector.cpp)
target_link_libraries(torbox PRIVATE KMagMuxCore Qt6::Core Qt6::Network)

# Ensure the plugin is built to the KMagMux plugins folder
set_target_properties(torbox PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

install(TARGETS torbox DESTINATION ${CMAKE_INSTALL_LIBDIR}/kmagmux/plugins)
