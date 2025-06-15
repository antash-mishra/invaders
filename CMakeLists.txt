cmake_minimum_required(VERSION 3.10)
project(space_shooter VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set library paths for Windows cross-compilation
if(CMAKE_CROSSCOMPILING)
    set(GLFW_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/win-libs/glfw/lib-mingw-w64)
    set(GLFW_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/win-libs/glfw/include)
    set(OPENAL_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/win-libs/openal/libs/Win64)
    set(OPENAL_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/win-libs/openal/include)
    set(ASSIMP_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/win-libs/assimp/lib)
    set(ASSIMP_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/win-libs/assimp/include)
endif()

# Source files
add_executable(space_shooter 
    src/main.cpp 
    src/glad.c
    src/stb_image.cpp
    src/shader.cpp
    src/camera.cpp
    src/model.cpp
    src/mesh.cpp
    src/audio_manager.cpp
)

target_include_directories(space_shooter PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENAL_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
    ${ASSIMP_INCLUDE_DIR}
)

# Find and link libraries
if(CMAKE_CROSSCOMPILING)
    target_link_libraries(space_shooter 
        ${GLFW_LIBRARY_DIR}/libglfw3.a
        ${ASSIMP_LIBRARY_DIR}/libassimp.dll.a
        ${OPENAL_LIBRARY_DIR}/libOpenAL32.dll.a
        gdi32
        stdc++fs
    )
    # Copy required DLLs to the build directory
    add_custom_command(TARGET space_shooter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ASSIMP_LIBRARY_DIR}/../bin/libassimp-5.dll"
        $<TARGET_FILE_DIR:space_shooter>
    )
    add_custom_command(TARGET space_shooter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/win-libs/openal/bin/Win64/soft_oal.dll"
        $<TARGET_FILE_DIR:space_shooter>
    )
else()
    find_package(assimp REQUIRED)
    find_package(OpenAL REQUIRED)
    target_link_libraries(space_shooter 
        glfw
        assimp::assimp
        ${OPENAL_LIBRARY}
    )
endif()
