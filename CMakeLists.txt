cmake_minimum_required(VERSION 3.18)
project(farm_control CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# compile_commands.json in the build folder, for the editor (.vscode/c_cpp_properties.json)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Device config for this machine: API/src/config/config_<DEVICE_CONFIG>.cpp
#   cmake -B build -DDEVICE_CONFIG=uppertank
# No default on purpose: building the wrong config on a RevPi drives the wrong outputs.
set(DEVICE_CONFIG "" CACHE STRING "Device config to build (config_<name>.cpp in API/src/config)")

find_package(Threads REQUIRED)

# FTXUI for the console, fetched at a pinned release (first configure needs
# internet). Before the warning flags below, so they only apply to our code.
include(FetchContent)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v6.1.9
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(ftxui)

add_compile_options(-Wall -Wextra)

# One library per module, each described in <module>/<module>.cmake. They are
# include()d, not add_subdirectory()'d, so there's no build folder per module:
# every library (lib<name>.a) and program lands in build/.
# Machine specific (depend on DEVICE_CONFIG): farm (API/) and ro (RO/).
# The same on every machine: comms, scheduler, procedure, notifications (host_pc only).
include(comms/comms.cmake)              # comms
include(scheduler/scheduler.cmake)      # scheduler
include(API/api.cmake)                  # farm
include(RO/ro.cmake)                    # ro
include(procedure/procedure.cmake)      # procedure
include(notifications/notifications.cmake)   # notifications, ntfy_send (host_pc)

# Programs
include(user_space/user_space.cmake)    # main_<DEVICE_CONFIG> and other user_space/src/*.cpp
include(console/console.cmake)          # ro_console
