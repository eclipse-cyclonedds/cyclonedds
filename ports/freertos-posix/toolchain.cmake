set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR "${CMAKE_SYSTEM_HOST_PROCESSOR}")

set(include_paths "-I${CMAKE_CURRENT_LIST_DIR}/lwip -I${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Sim/Source/include -I${CMAKE_CURRENT_LIST_DIR}/include -I${CMAKE_CURRENT_LIST_DIR}/build/install/FreeRTOS-Sim/include -I${CMAKE_CURRENT_LIST_DIR}/lwip/lwip-contrib/ports/unix/lib -I${CMAKE_CURRENT_LIST_DIR}/lwip/lwip-contrib/ports/unix/port/include -I${CMAKE_CURRENT_LIST_DIR}/lwip/lwip/src/include")

set(library_paths "-L${CMAKE_CURRENT_LIST_DIR}/build/install/FreeRTOS-Sim/lib -L${CMAKE_CURRENT_LIST_DIR}/lwip/lwip-contrib/ports/unix/example_app/build")

set(c_libraries "-lfreertos-sim -lfreertos-sim-loader -lpthread -llwipcore -llwipcontribportunix")
set(cxx_libraries "${c_libraries}")

set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/bin/g++")

set(CMAKE_C_FLAGS "${include_paths} ${c_flags} ${defines}")
set(CMAKE_CXX_FLAGS "${include_paths} ${cxx_flags} ${defines}")
set(CMAKE_EXE_LINKER_FLAGS "${linker_flags} ${library_paths} -Wl,-rpath -Wl,${CMAKE_CURRENT_LIST_DIR}/lwip/lwip-contrib/ports/unix/example_app/build")
set(CMAKE_C_STANDARD_LIBRARIES "${c_objects} ${library_paths} ${c_libraries}")
set(CMAKE_CXX_STANDARD_LIBRARIES "${cxx_objects} ${library_paths} ${cxx_libraries}")

set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
