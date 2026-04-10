project(RealDebridPlugin)

add_library(realdebrid MODULE RealDebridConnector.cpp)
target_link_libraries(realdebrid PRIVATE KMagMuxCore Qt6::Core Qt6::Network)

# Ensure the plugin is built to the KMagMux plugins folder
set_target_properties(realdebrid PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

install(TARGETS realdebrid DESTINATION ${CMAKE_INSTALL_LIBDIR}/kmagmux/plugins)
