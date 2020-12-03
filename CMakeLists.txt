project (blender-convert CXX)
cmake_minimum_required (VERSION 3.3)

include_directories(lib/kaitai_struct_cpp_stl_runtime)

add_subdirectory(lib/kaitai_struct_cpp_stl_runtime)
add_subdirectory(src)
