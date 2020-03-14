# Toolchain file for building 32-bit Linux libraries

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
set(CMAKE_LIBRARY_PATH "/usr/lib/i386-linux-gnu")