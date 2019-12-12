# Building `codepad`

`codepad` uses CMake as its build system. `codepad` currently supports only Windows, with an outdated Linux port.

## Prerequisites

- CMake

### Dependencies

- RapidJSON

### On Windows

- Visual Studio 2017 with proper C++17 support (15.7 or later is recommended), or 2019.

Building with MinGW-w32 can be more tricky due to potentially missing Windows SDK symbols, and is therefore not recommended.

### On Linux

*The linux port does not build currently.*

- GTK 3
- FontConfig

## Build

The procedure of building `codepad` using CMake is fairly standard.

## Building the documentation

Use doxygen and `doxyfile` in the root directory to build the documentation. Although many efforts have been made to keep the documentation correct and up-to-date, you'll likely encounter warnings about invalid references.
