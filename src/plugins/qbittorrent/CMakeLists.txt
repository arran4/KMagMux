project(QBittorrentPlugin)

add_library(qbittorrent MODULE QBittorrentConnector.cpp)
target_link_libraries(qbittorrent PRIVATE KMagMuxCore Qt6::Core Qt6::Network)

# Ensure the plugin is built to the KMagMux plugins folder
set_target_properties(qbittorrent PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

install(TARGETS qbittorrent DESTINATION ${CMAKE_INSTALL_LIBDIR}/kmagmux/plugins)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
