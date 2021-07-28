# Building the engine
## Requirements
1. You need to install the Vulkan SDK

## Steps

Git clone this repository, with the option `--recurse-submodules` active.

Go into `Carrot/thirdparty/OpenXR-SDK-Source`. 
Create a subdirectory `build/win64` (replace `win64` by your platform), cd into it and launch `cmake -G "Visual Studio 2019 6" -A x64 ../..` (you can replace `Visual Studio 2019 6` with your generator of choice if you know how to build with it).

If you are on Windows and selected the Visual Studio CMake generator, open the generated solution `OPENXR.sln` inside build/win64 and build via Visual Studio.