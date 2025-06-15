# toolchain-mingw.cmake

# Set the target system name
set(CMAKE_SYSTEM_NAME Windows)

# Set the cross-compilers
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Set the root for the cross-compiler toolchain
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Set the library paths for the cross-compiled libraries
set(CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/win-libs)

# Modify find behavior
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
