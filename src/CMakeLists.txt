set (SOURCES
    test_app.cpp
)

add_executable(${PROJECT_NAME} test_app.cpp)

target_link_libraries (${PROJECT_NAME} kaitai_struct_cpp_stl_runtime)