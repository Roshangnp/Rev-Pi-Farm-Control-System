# RO permeate flow Newton iteration test, on the RO RevPi. Standalone project: pulls in the repo's
# libraries with the ro config.
#   cmake -S tests/ro_newton_test -B tests/ro_newton_test/build
#   cmake --build tests/ro_newton_test/build -j4 --target ro_newton_test
cmake_minimum_required(VERSION 3.18)
project(ro_newton_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DEVICE_CONFIG "ro" CACHE STRING "Always the RO RevPi" FORCE)
add_subdirectory(../.. farm_control EXCLUDE_FROM_ALL)

add_executable(ro_newton_test ro_newton_test.cpp)
target_compile_options(ro_newton_test PRIVATE -Wall -Wextra)
target_link_libraries(ro_newton_test PRIVATE ro_control)
