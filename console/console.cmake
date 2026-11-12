# Console: ro_console, a keyboard-only terminal UI for the whole farm. Connects only
# to this machine's own service on 127.0.0.1 (main_*), which serves every device and
# the RO. Uses only client classes: remote_* devices (farm), remote_ro (ro),
# remote_procedure (procedure), remote_link (comms); the scheduler only for
# procedure.h's headers. Static libraries only contribute what it uses, so it gets
# none of the machine config, piControl or WiseConn and is the same program on every
# machine. Rule: console code never uses a config.h device or an ro_config.h
# function, or the machine config (and its threads) would be linked in.
add_executable(ro_console
    ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/console_devices.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/console_ui.cpp)
target_include_directories(ro_console PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(ro_console PRIVATE farm ro procedure scheduler comms
                      ftxui::screen ftxui::dom ftxui::component)
