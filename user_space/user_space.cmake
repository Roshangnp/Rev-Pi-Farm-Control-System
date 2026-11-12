# User space: one program per user_space/src/*.cpp, named after the file.
# main_<config>.cpp is only built with that config; other files with every config.
# Every program links this machine's procedure, ro and farm (API) libraries and the shared ones.
file(GLOB user_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp)
foreach(source ${user_sources})
    get_filename_component(name ${source} NAME_WE)
    if(name MATCHES "^main_(.+)$" AND NOT CMAKE_MATCH_1 STREQUAL DEVICE_CONFIG)
        continue()
    endif()
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE procedure ro farm scheduler comms)
endforeach()
if(TARGET main_host_pc)
    target_link_libraries(main_host_pc PRIVATE notifications)
endif()
