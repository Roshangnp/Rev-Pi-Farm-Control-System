# ro (libro.a): the RO for this machine. Machine specific: this machine's RO config
# (src/config/ro_config_<DEVICE_CONFIG>.cpp, the ro_config.h functions), ro_server
# (serves them as "ro" / "ro_procedure") and remote_ro (client). The ro state
# machine (src/ro.cpp) is only built for the ro config, so no second controller can
# run elsewhere; other configs reach it through remote_ro. A config without an RO
# config (test) gets only ro_server and remote_ro. RO/src is public for the
# "remote_ro/..." headers the console includes.
set(ro_dir ${CMAKE_CURRENT_LIST_DIR})
set(ro_sources ${ro_dir}/src/remote_ro/ro_server.cpp ${ro_dir}/src/remote_ro/remote_ro.cpp)
set(ro_config_file ${ro_dir}/src/config/ro_config_${DEVICE_CONFIG}.cpp)
if(EXISTS ${ro_config_file})
    list(APPEND ro_sources ${ro_config_file})
endif()
if(DEVICE_CONFIG STREQUAL "ro")
    list(APPEND ro_sources ${ro_dir}/src/ro.cpp)
endif()
add_library(ro STATIC ${ro_sources})
target_include_directories(ro PUBLIC ${ro_dir}/include ${ro_dir}/src)
# ro owns a procedure and procedure calls the ro_config.h functions, so the two
# static libraries depend on each other; CMake repeats them on the link line
target_link_libraries(ro PUBLIC procedure farm scheduler comms)
