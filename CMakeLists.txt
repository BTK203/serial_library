cmake_minimum_required(VERSION 3.15)
project(serial_library)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-unused-function)
endif()

# GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# this ensures that all symbols are properly exported if using Windows. Without this, programs linking the library
# will fail to find the .lib import library
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

#
# Requirements
#
find_package(ament_cmake QUIET)
find_package(rclcpp QUIET)
find_package(std_msgs QUIET)
find_package(Threads REQUIRED)

#
# serial_library subdirectory compilation file
#
set(THREADS_PREFER_PTHREAD_FLAG ON)

#
# Compile src files
#

file(GLOB serlib_src src/*.cpp)
add_library(serial_library SHARED ${serlib_src})

# gets rid of annoying automoc warning that qt likes to throw
set_target_properties(serial_library PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF)

target_include_directories(serial_library PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

if(rclcpp_FOUND AND std_msgs_FOUND)
    add_definitions(-DUSE_ROS)

    if(ament_cmake_FOUND)
        ament_target_dependencies(
            serial_library
            rclcpp
            std_msgs)
    endif()
endif()

#add_executable(transceiver_bridge src/tools/TransceiverBridge.cpp)

#set_target_properties(transceiver_bridge PROPERTIES
#    AUTOMOC OFF
#    AUTOUIC OFF
#    AUTORCC OFF)

#target_link_libraries(transceiver_bridge serial_library)

install(TARGETS serial_library
        EXPORT serial_libraryTargets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

# install(TARGETS serial_library
#         EXPORT serial_libraryTargets
#         LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#         RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

#install(TARGETS transceiver_bridge
#    DESTINATION lib/${PROJECT_NAME})

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
    
    # get GTest (https://github.com/google/googletest/tree/main/googletest#incorporating-into-an-existing-cmake-project)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    file(GLOB test_src test/*.cpp)
    add_executable(test_serial_library ${test_src})
    target_include_directories(test_serial_library PUBLIC include)
    target_link_libraries(test_serial_library
        PUBLIC gtest_main serial_library
        PRIVATE Threads::Threads)
    install(TARGETS test_serial_library
        DESTINATION lib/${PROJECT_NAME})
endif()

if(ament_cmake_FOUND)
    ament_package()
endif()
