## Building codepad

- Install prerequisites:
  - On Windows, Visual Studio 2019 is recommended. You can find installers of CMake and VS2019 online. Building with MinGW-w32 has not been thoroughly tested and can be tricky.

    The best way to install other required packages is to use `vcpkg`. You can install it following the instructions [here](https://github.com/microsoft/vcpkg#quick-start-windows). After that, install the following packages:
    ```
    vcpkg install rapidjson
    vcpkg install pybind11
    vcpkg install uriparser
    ```

    If you want to build codepad with Cairo support, also install:
    ```
    vcpkg install fontconfig
    vcpkg install freetype
    vcpkg install harfbuzz
    vcpkg install pango
    vcpkg install cairo
    ```
    The Cairo renderer can suffer from poor compatibility and performance on Windows. Specify `-DUSE_CAIRO=No` while configuring to build without the Cairo backend.

  - On Ubuntu, install CMake and g++:
    ```
    apt install cmake
    apt install g++-10
    ```

    The packages can be installed using `apt`:
    ```
    apt install rapidjson-dev
    apt install pybind11-dev
    apt install libfontconfig1-dev
    apt install libfreetype-dev
    apt install libharfbuzz-dev
    apt install libpango1.0-dev
    apt install libcairo2-dev
    apt install libgtk-3-dev
    apt install liburiparser-dev
    ```
    Note that older versions of pybind11 may not be compatible with codepad, in which case the plugin will fail to build, but you should still be able to build and run the rest of the program without any problem.

- Clone the repository and initialize submodules:
  ```
  git clone https://github.com/lukedan/codepad.git
  git submodule update --init --recursive
  ```

- Generate and build:
  ```
  mkdir build
  cd build
  cmake ..
  cmake -build .
  ```
  If you're building on Windows using `vcpkg`, also specify `-DCMAKE_TOOLCHAIN_FILE=[path to vcpkg]/scripts/buildsystems/vcpkg.cmake` on line 3. If you're buliding without Cairo, specify `-DUSE_CAIRO=No` on line 3.

## Running codepad

- Codepad uses configuration files in the `config` folder. Currently this dependency is hard-coded in [`src/main.cpp`](src/main.cpp), and the `config` folder must be in the working directory of the executable.
- `codepad` loads plugins specified in [`config/settings.json`](config/settings.json) that are shared libraries, and due to how shared library loading works on different platforms, the system may fail to find the library files. Thus, you may need to manually modify the list items to match the relative paths of the library files in the build output folder. By default the plugin shared libraries are placed in the same folder as the executable.
- You may need to change the rendering backend in `config/settings.json` to `cairo` on Linux.

## Building the documentation

Use doxygen and `doxyfile` in the root directory to build the documentation. Although many efforts have been made to keep the documentation correct and up-to-date, it may still contain outdated information and/or invalid references to identifiers.
