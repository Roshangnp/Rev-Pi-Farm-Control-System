# User space: one program per user_space/src/*.cpp, named after the file.
# main_<config>.cpp is only built with that config; other files with every config.
# Every program links this machine's farm (API) and ro libraries and the shared ones.
file(GLOB user_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
foreach(source ${user_sources})
    get_filename_component(name ${source} NAME_WE)
    if(name MATCHES "^main_(.+)$" AND NOT CMAKE_MATCH_1 STREQUAL DEVICE_CONFIG)
        continue()
    endif()
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ro procedure farm scheduler comms)
    # The procedure instances this machine doesn't own (main_ro owns ro_procedure;
    # test has no RO RevPi connection)
    if(DEVICE_CONFIG STREQUAL "host_pc" OR DEVICE_CONFIG STREQUAL "uppertank")
        target_link_libraries(${name} PRIVATE procedure_remote)
    endif()
endforeach()
if(TARGET main_host_pc)
    target_link_libraries(main_host_pc PRIVATE notifications)
endif()
