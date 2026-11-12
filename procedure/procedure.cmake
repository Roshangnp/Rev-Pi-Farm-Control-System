# procedure (libprocedure.a): farm procedures for this machine. Machine specific,
# like farm and ro: the procedure core (procedure, procedure_server,
# remote_procedure) plus this machine's procedure config
# (src/config/procedure_config_<DEVICE_CONFIG>.cpp: binds procedure_config.h's
# instances, creates and serves the ones it owns in addProcedures()). A config
# without a procedure config (test) gets only the core.
# procedure_client (libprocedure_client.a), for ro_console: every instance as a
# remote_procedure (src/config/procedure_client_config.cpp) on farm_client's
# connections; nothing owned or served.
set(procedure_dir ${CMAKE_CURRENT_LIST_DIR})
set(procedure_sources
    ${procedure_dir}/src/procedure.cpp
    ${procedure_dir}/src/remote_procedure/procedure_server.cpp
    ${procedure_dir}/src/remote_procedure/remote_procedure.cpp)
set(procedure_config_file ${procedure_dir}/src/config/procedure_config_${DEVICE_CONFIG}.cpp)
if(EXISTS ${procedure_config_file})
    list(APPEND procedure_sources ${procedure_config_file})
endif()
add_library(procedure STATIC ${procedure_sources})
target_include_directories(procedure PUBLIC ${procedure_dir}/include ${procedure_dir}/src)
# The procedure steps use config.h devices and the ro_config.h functions
target_link_libraries(procedure PUBLIC ro farm scheduler comms)

add_library(procedure_client STATIC
    ${procedure_dir}/src/remote_procedure/remote_procedure.cpp
    ${procedure_dir}/src/config/procedure_client_config.cpp)
target_include_directories(procedure_client PUBLIC ${procedure_dir}/include ${procedure_dir}/src)
target_link_libraries(procedure_client PUBLIC farm_client scheduler comms)
