# scheduler: tasks run when their conditions are met (system clock). Same on every machine.
add_library(scheduler STATIC ${CMAKE_CURRENT_LIST_DIR}/src/scheduler.cpp)
target_include_directories(scheduler PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
