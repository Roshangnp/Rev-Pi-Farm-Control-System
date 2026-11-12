# comms: line protocol between farm machines (line_server, remote_link, port 47100).
# Same on every machine.
add_library(comms STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/line_server.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/remote_link.cpp)
target_include_directories(comms PUBLIC ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(comms PUBLIC Threads::Threads)
