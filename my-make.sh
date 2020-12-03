
cd /home/bjorn/Desktop/blender-convert && /usr/bin/cmake -S/home/bjorn/Desktop/blender-convert -B/home/bjorn/Desktop/blender-convert --check-build-system CMakeFiles/Makefile.cmake 0

cd /home/bjorn/Desktop/blender-convert && /usr/bin/cmake -E cmake_progress_start /home/bjorn/Desktop/blender-convert/CMakeFiles /home/bjorn/Desktop/blender-convert/src/CMakeFiles/progress.marks

cd /home/bjorn/Desktop/blender-convert && make -f CMakeFiles/Makefile2 src/all

make -f lib/kaitai_struct_cpp_stl_runtime/CMakeFiles/kaitai_struct_cpp_stl_runtime.dir/build.make lib/kaitai_struct_cpp_stl_runtime/CMakeFiles/kaitai_struct_cpp_stl_runtime.dir/depend

cd /home/bjorn/Desktop/blender-convert && /usr/bin/cmake -E cmake_depends "Unix Makefiles" /home/bjorn/Desktop/blender-convert /home/bjorn/Desktop/blender-convert/lib/kaitai_struct_cpp_stl_runtime /home/bjorn/Desktop/blender-convert /home/bjorn/Desktop/blender-convert/lib/kaitai_struct_cpp_stl_runtime /home/bjorn/Desktop/blender-convert/lib/kaitai_struct_cpp_stl_runtime/CMakeFiles/kaitai_struct_cpp_stl_runtime.dir/DependInfo.cmake --color=

make -f lib/kaitai_struct_cpp_stl_runtime/CMakeFiles/kaitai_struct_cpp_stl_runtime.dir/build.make lib/kaitai_struct_cpp_stl_runtime/CMakeFiles/kaitai_struct_cpp_stl_runtime.dir/build

/usr/bin/cmake -E cmake_echo_color --switch= --progress-dir=/home/bjorn/Desktop/blender-convert/CMakeFiles --progress-num=3,4 "Built target kaitai_struct_cpp_stl_runtime"

make -f src/CMakeFiles/blender-convert.dir/build.make src/CMakeFiles/blender-convert.dir/depend

cd /home/bjorn/Desktop/blender-convert && /usr/bin/cmake -E cmake_depends "Unix Makefiles" /home/bjorn/Desktop/blender-convert /home/bjorn/Desktop/blender-convert/src /home/bjorn/Desktop/blender-convert /home/bjorn/Desktop/blender-convert/src /home/bjorn/Desktop/blender-convert/src/CMakeFiles/blender-convert.dir/DependInfo.cmake --color=

make -f src/CMakeFiles/blender-convert.dir/build.make src/CMakeFiles/blender-convert.dir/build

/usr/bin/cmake -E cmake_echo_color --switch= --green --bold --progress-dir=/home/bjorn/Desktop/blender-convert/CMakeFiles --progress-num=2 "Linking CXX executable blender-convert"

cd /home/bjorn/Desktop/blender-convert/src && /usr/bin/cmake -E cmake_link_script CMakeFiles/blender-convert.dir/link.txt --verbose=

/usr/bin/cmake -E cmake_echo_color --switch= --progress-dir=/home/bjorn/Desktop/blender-convert/CMakeFiles --progress-num=1,2 "Built target blender-convert"

/usr/bin/cmake -E cmake_progress_start /home/bjorn/Desktop/blender-convert/CMakeFiles 0
