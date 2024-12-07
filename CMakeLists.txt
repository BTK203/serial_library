#
# serial_library subdirectory compilation file
#

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#
# Compile src files
#

file(GLOB serlib_src src/*.cpp)
add_library(serial_library ${serlib_src})
target_include_directories(serial_library PUBLIC include)

#
# Compile test files
#


file(GLOB test_src test/*.cpp)
add_executable(test_serial_library ${test_src})
target_include_directories(test_serial_library PUBLIC include)
target_link_libraries(test_serial_library
    PUBLIC gtest serial_library
    PRIVATE Threads::Threads)
