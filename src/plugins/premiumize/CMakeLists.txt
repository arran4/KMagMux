project(PremiumizePlugin)

add_library(premiumize MODULE PremiumizeConnector.cpp)
target_link_libraries(premiumize PRIVATE KMagMuxCore Qt5::Core Qt5::Network)

# Ensure the plugin is built to the KMagMux plugins folder
set_target_properties(premiumize PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

install(TARGETS premiumize DESTINATION ${CMAKE_INSTALL_LIBDIR}/kmagmux/plugins)
