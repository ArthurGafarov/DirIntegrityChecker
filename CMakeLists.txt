cmake_minimum_required(VERSION 3.0.2)

project(dir_checker VERSION 0.0.1)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -pedantic -Wall")

find_package(Boost 1.66.0 REQUIRED COMPONENTS program_options)

add_executable(
    ${PROJECT_NAME}
    main.cpp
    app/crc32.cpp
    app/dir_scanner.cpp
    app/periodic_task.cpp
    app/thread_pool_queue.cpp
    app/watcher.cpp
)

target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
