@echo off
REM Build script for ParticleLifeGL using MinGW-w64
REM Requires: mingw-w64, GLFW, GLEW installed via pacman or similar

set CXX=g++
set CXXFLAGS=-std=c++17 -O2 -Wall
set LDFLAGS=-lglfw3 -lglew32 -lopengl32 -lgdi32

echo Compiling...
%CXX% %CXXFLAGS% -c src/simulation.cpp -o simulation.o
%CXX% %CXXFLAGS% -c src/renderer.cpp -o renderer.o
%CXX% %CXXFLAGS% -c src/main.cpp -o main.o
%CXX% simulation.o renderer.o main.o %LDFLAGS% -o ParticleLifeGL.exe

echo Done.
if exist ParticleLifeGL.exe (
    echo Build successful: ParticleLifeGL.exe
) else (
    echo Build FAILED
)
