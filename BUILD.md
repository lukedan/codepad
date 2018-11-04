# Building Codepad

Codepad currently supports only Windows and Linux. Development is mostly done under windows so far, so the Linux port can be a bit buggy.

## Prerequisites

Most dependencies listed below can be downloaded with a package manager, like `apt` or [`vcpkg`](https://github.com/Microsoft/vcpkg).

- FreeType
- RapidJSON
- Some additional OpenGL headers like [glext.h](https://www.khronos.org/registry/OpenGL/api/GL/glext.h). See [this answer](https://stackoverflow.com/questions/3933027/how-to-get-the-gl-library-headers/3939495#3939495) for more information.

### On Windows

- Visual Studio 2017 with proper C++17 support. Visual Studio 15.7 or later is preferred, while earlier versions may or may not do.

Building on Windows with CMake and MinGW-w32 is more tricky due to potentially missing Windows SDK symbols in MinGW-w32, and is not recommended. However, should the need arise, the procedure is practically identical to that on Linux.

### On Linux

- GTK 3
- FontConfig
- CMake

## Build

### On Windows

1. Open `codepad.sln` with Visual Studio 2017.
2. Build the solution using the ReleaseNoConsole configuration. Other configurations may also be useful for developers. You may need to change the Windows SDK version in project settings.

### On Linux

This application can be built under Linux using the standard procedure of building an application with CMake, described below.

1. Create a folder for building, and use cmake to generate the Makefile. For example, if you create a folder named `build` in the same folder as `CMakeLists.txt`, then you'll need to go into that folder and execute the command `cmake ..` from the console. Add the flag `-DCMAKE_BUILD_TYPE=Release` to generate Makefile for the release build.
2. Execute the command `make` under the same directory.

## Building the documentation

Use doxygen and `doxyfile` in the root directory to build the documentation. Although many efforts have been made to keep the documentation correct and up-to-date, you'll likely encounter warnings about invalid references.

## If it fails...

Should you encounter any problems during the build process, please let us know in the issues.
