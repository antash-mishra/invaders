cmake_minimum_required(VERSION 3.10)
project(space_shooter VERSION 0.1.0)

# Find Assimp package
find_package(assimp REQUIRED)

# Source files
add_executable(space_shooter 
    src/main.cpp 
    src/glad.c
    src/stb_image.cpp
    src/shader.cpp
    src/camera.cpp
    src/model.cpp
    src/mesh.cpp
)

target_include_directories(space_shooter PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Find and link libraries
target_link_libraries(space_shooter 
    glfw
    assimp::assimp  # Link Assimp using the target provided by find_package
)
