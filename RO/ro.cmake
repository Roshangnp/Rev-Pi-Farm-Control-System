# ro (libro.a): the RO for this machine. Machine specific: this machine's RO config
# (src/config/ro_config_<DEVICE_CONFIG>.cpp, the ro_config.h functions), ro_server
# (the RO RevPi serves them as "ro" / "ro_procedure") and remote_ro (client). The ro
# state machine (src/ro.cpp) is only built for the ro config, so no second controller
# can run elsewhere; other configs reach it through remote_ro.
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
# ro owns a procedure (procedure's steps call back into this library's
# ro_config.h functions, already linked by then)
target_link_libraries(ro PUBLIC procedure farm scheduler comms)

# ro_client (libro_client.a), for ro_console: the ro_config.h functions through
# remote_ro / remote_procedure on farm_client's RO RevPi connection; no
# controller and nothing served
add_library(ro_client STATIC ${ro_dir}/src/remote_ro/remote_ro.cpp ${ro_dir}/src/config/ro_client_config.cpp)
target_include_directories(ro_client PUBLIC ${ro_dir}/include ${ro_dir}/src)
target_link_libraries(ro_client PUBLIC procedure farm_client scheduler comms)
