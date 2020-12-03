set (SOURCES
    blender_blend.cpp
    test_app.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries (${PROJECT_NAME} kaitai_struct_cpp_stl_runtime)