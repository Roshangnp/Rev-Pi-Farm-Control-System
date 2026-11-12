# Console: ro_console, a keyboard-only terminal UI for the whole farm. A user-space
# program like main_host_pc: every device from config.h and the RO from ro_config.h,
# through the client variants farm_client / ro_client (every device remote on a
# connection to its owner, 127.0.0.1 for this machine's own RevPi; WiseConn from
# this machine's main), and the procedures from procedure_config.h (procedure_remote,
# every instance remote to its owner). Never links farm or ro, so it never touches hardware or runs
# a controller, and it needs the owners' main_* running.
add_executable(ro_console
    ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/console_devices.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/console_ui.cpp)
target_include_directories(ro_console PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
# The client config's addresses, for the header (RO@<host>, Upper tank@<host>)
target_compile_definitions(ro_console PRIVATE FARM_RO_HOST="${client_ro_host}" FARM_UPPER_HOST="${client_upper_host}")
target_link_libraries(ro_console PRIVATE ro_client procedure_remote procedure farm_client scheduler comms
                      ftxui::screen ftxui::dom ftxui::component)
