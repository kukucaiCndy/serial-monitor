set(GUI_SOURCES
    main.cpp
    main_window.cpp
    serial_tab_widget.cpp
    log_view.cpp
    send_panel.cpp
    status_bar.cpp
    settings_dialog.cpp
    ipc_server.cpp
)

set(GUI_HEADERS
    main_window.h
    serial_tab_widget.h
    log_view.h
    send_panel.h
    status_bar.h
    settings_dialog.h
    ipc_server.h
)

add_executable(serial-monitor
    ${GUI_SOURCES}
    ${GUI_HEADERS}
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)

target_include_directories(serial-monitor PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
    ${SPDLOG_INCLUDE_DIR}
)

target_link_libraries(serial-monitor PRIVATE
    serialmonitor_core
    Qt5::Widgets
    Qt5::SerialPort
    Qt5::Network
)

if(WIN32)
    target_link_libraries(serial-monitor PRIVATE dwmapi)
endif()

set_target_properties(serial-monitor PROPERTIES
    WIN32_EXECUTABLE TRUE
)
