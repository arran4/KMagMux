project(PutIoPlugin)

add_library(putio MODULE PutIoConnector.cpp)
target_link_libraries(putio PRIVATE KMagMuxCore Qt6::Core Qt6::Network)

# Ensure the plugin is built to the KMagMux plugins folder
set_target_properties(putio PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

install(TARGETS putio DESTINATION ${CMAKE_INSTALL_LIBDIR}/kmagmux/plugins)
