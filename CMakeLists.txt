project (blender-convert CXX)
cmake_minimum_required (VERSION 3.3)

#include_directories(lib/kaitai)
#link_directories(lib/kaitai)
add_subdirectory(lib/kaitai)

set (CMAKE_INCLUDE_CURRENT_DIR ON)

set (SOURCES
    src/test_app.cpp
)

add_executable(${PROJECT_NAME} src/test_app.cpp)

#target_link_libraries (${PROJECT_NAME} lib/kaitai)