# procedure (libprocedure.a): farm procedures (procedure), their server
# (procedure_server) and client (remote_procedure). Same on every machine; the
# procedure steps use config.h devices and the ro_config.h functions, resolved
# from this machine's farm and ro libraries (only linked where a program uses them).
set(procedure_dir ${CMAKE_CURRENT_LIST_DIR})
add_library(procedure STATIC
    ${procedure_dir}/src/procedure.cpp
    ${procedure_dir}/src/remote_procedure/procedure_server.cpp
    ${procedure_dir}/src/remote_procedure/remote_procedure.cpp)
target_include_directories(procedure PUBLIC ${procedure_dir}/include ${procedure_dir}/src)
target_link_libraries(procedure PUBLIC ro farm scheduler comms)
