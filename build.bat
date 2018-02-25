@echo off

if not exist build mkdir build

if not exist glfw\build (
   cd glfw
   mkdir build
   cd build
   cmake .. -G "Visual Studio 14 2015 Win64" -DCMAKE_BUILD_TYPE=Release -DGLFW_USE_RETINA=1 -DCMAKE_STATIC_LINKER_FLAGS=/machine:x64 -DCMAKE_EXE_LINKER_FLAGS=/machine:x64 -DCMAKE_MODULE_LINKER_FLAGS=/machine:x64
   msbuild GLFW.sln /p:Configuration=Release  /p:Platform=x64 /M
   cd ..\..
)

set glfw_lib=..\glfw\build\src\Release\glfw3.lib
cd build
cl.exe /Wall /Zi /MD /FC /Od /I..\glfw\include ..\timer.c %glfw_lib% OpenGL32.lib Gdi32.lib ole32.lib User32.lib Shell32.lib
cd ..
