# Building `codepad`

`codepad` uses CMake. Due to the usage of C++20, it is recommended that you use the latest versions of compilers to avoid language support issues.

## Prerequisites

- CMake

### Dependencies

- RapidJSON

#### Graphics Backend Dependencies

`codepad` supports numerous graphics backends, including Direct2D and Cairo. On Linux, only the Cairo renderer is supported. On Windows the Direct2D renderer is recommended, although you can still build with Cairo renderer support by specifying `-DUSE_CAIRO=YES` for CMake. The Cairo renderer can suffer from poor compatibility and performance on Windows.

The Cairo renderer, if enabled, requires the following packages:

- FontConfig
- Cairo
- FreeType
- HarfBuzz
- Pango

#### Additional Linux Dependencies

- GTK 3

#### Native Plugins

- The `python_plugin_host_pybind11` plugin depends on `pybind11`.

## Building on Windows

The recommended building configuration for windows is:

- Visual Studio 2019.
- `vcpkg` for dependency installation, especially if you want to build with Cairo renderer support. Refer to `vcpkg` documentation for how to make the packages visible to CMake.

Building with MinGW-w32 has not been thoroughly tested and can be tricky.

## Building the documentation

Use doxygen and `doxyfile` in the root directory to build the documentation. Although many efforts have been made to keep the documentation correct and up-to-date, it may still contain outdated information and/or invalid references to identifiers.
