cmake_minimum_required(VERSION 3.10)
project(ParticleLifeGL)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

add_executable(ParticleLifeGL
    src/main.cpp
    src/simulation.cpp
    src/renderer.cpp
)

target_include_directories(ParticleLifeGL PRIVATE src)
target_link_libraries(ParticleLifeGL PRIVATE OpenGL::GL glfw GLEW)
