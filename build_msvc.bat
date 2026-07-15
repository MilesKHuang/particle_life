@echo off
REM Build script for ParticleLifeGL using MSVC (Visual Studio 2019)
REM 
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set NUGET_PACKAGES=C:\Users\Administrator\.nuget\packages
set INCLUDE_DIRS=/I "%NUGET_PACKAGES%\nupengl.core\0.1.0.1\build\native\include"
set LIB_DIRS=/LIBPATH:"%NUGET_PACKAGES%\nupengl.core\0.1.0.1\build\native\lib\x64"
set LIBS=glfw3dll.lib glew32.lib opengl32.lib

echo Compiling simulation.cpp...
cl /nologo /std:c++17 /O2 /EHsc %INCLUDE_DIRS% /c src\simulation.cpp /Fo:simulation.obj
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo Compiling renderer.cpp...
cl /nologo /std:c++17 /O2 /EHsc %INCLUDE_DIRS% /c src\renderer.cpp /Fo:renderer.obj
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo Compiling main.cpp...
cl /nologo /std:c++17 /O2 /EHsc %INCLUDE_DIRS% /c src\main.cpp /Fo:main.obj
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo Linking...
link /nologo /OUT:ParticleLifeGL.exe simulation.obj renderer.obj main.obj %LIB_DIRS% %LIBS%
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo Build successful: ParticleLifeGL.exe

REM Copy DLLs to output directory
copy /Y "%NUGET_PACKAGES%\nupengl.core.redist\0.1.0.1\build\native\bin\x64\glfw3.dll" . >nul
copy /Y "%NUGET_PACKAGES%\nupengl.core.redist\0.1.0.1\build\native\bin\x64\glew32.dll" . >nul
echo DLLs copied.
