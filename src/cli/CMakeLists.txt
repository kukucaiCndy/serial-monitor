set(CLI_SOURCES
    main.cpp
    cli_app.cpp
    ipc_client.cpp
)

set(CLI_HEADERS
    cli_app.h
    ipc_client.h
)

add_executable(serial-monitor-cli
    ${CLI_SOURCES}
    ${CLI_HEADERS}
)

target_compile_definitions(serial-monitor-cli PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
)

target_include_directories(serial-monitor-cli PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${RAPIDJSON_INCLUDE_DIR}
    ${SPDLOG_INCLUDE_DIR}
)

target_link_libraries(serial-monitor-cli PRIVATE
    serialmonitor_core
    Qt5::Core
    Qt5::Network
)

set_target_properties(serial-monitor-cli PROPERTIES
    WIN32_EXECUTABLE FALSE
)
