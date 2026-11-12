# procedure (libprocedure.a): farm procedures (procedure), their server
# (procedure_server) and client (remote_procedure). Same on every machine. The
# procedure steps use config.h devices and the ro_config.h functions: only their
# headers here; the program links the farm and ro (or farm_client and ro_client)
# libraries that define them, so this library pulls in neither variant itself.
set(procedure_dir ${CMAKE_CURRENT_LIST_DIR})
add_library(procedure STATIC
    ${procedure_dir}/src/procedure.cpp
    ${procedure_dir}/src/remote_procedure/procedure_server.cpp
    ${procedure_dir}/src/remote_procedure/remote_procedure.cpp)
target_include_directories(procedure PUBLIC ${procedure_dir}/include ${procedure_dir}/src
                           PRIVATE ${CMAKE_SOURCE_DIR}/RO/include ${CMAKE_SOURCE_DIR}/API/include)
target_link_libraries(procedure PUBLIC scheduler comms)

# procedure_remote (libprocedure_remote.a): procedure_config.h's instances owned by
# another machine, as remote_procedure on this machine's connection to the owner
# (ro_revpi, from this machine's config). For programs that don't own them:
# main_host_pc, main_uppertank, the console. Not for the RO RevPi's main_ro, which
# owns ro_procedure.
add_library(procedure_remote STATIC ${procedure_dir}/src/config/procedure_remote_config.cpp)
target_link_libraries(procedure_remote PUBLIC procedure comms)
