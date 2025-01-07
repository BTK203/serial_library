cmake_minimum_required(VERSION 3.9)
project(serial_library)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# Requirements
#
find_package(ament_cmake)
find_package(Threads REQUIRED)

#
# serial_library subdirectory compilation file
#
set(THREADS_PREFER_PTHREAD_FLAG ON)

#
# Compile src files
#

file(GLOB serlib_src src/*.cpp)
add_library(serial_library ${serlib_src})

target_include_directories(serial_library PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

install(TARGETS serial_library
    EXPORT serial_libraryTargets
    DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY include/
    DESTINATION include)

#
# Export serial library
#
if(ament_cmake_FOUND)
    ament_export_targets(serial_libraryTargets HAS_LIBRARY_TARGET)
endif()

#
# Compile test files
#

if(BUILD_TESTING)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)

    file(GLOB test_src test/*.cpp)
    add_executable(test_serial_library ${test_src})
    target_include_directories(test_serial_library PUBLIC include)
    target_link_libraries(test_serial_library
        PUBLIC gtest serial_library
        PRIVATE Threads::Threads)
    install(TARGETS test_serial_library
        DESTINATION lib/${PROJECT_NAME})
endif()

if(ament_cmake_FOUND)
    ament_package()
endif()
