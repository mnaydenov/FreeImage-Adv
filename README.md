## What is changed ?

 - Added CMake support for the base library and its dependencies, located in the Source folder.
 - Fixed build issues, discovered using **mingw-w64** under Windows.

## How to use it ?

>Please note that for now, only the core library is supported (the `Source` directory).

The project can be configured, using the standard CMake procedures. 

From `Source`, one can simply execute: 
```
cmake -B <build-directory>
```
Where `<build-directory>` is the desired build destination directory. 
This will create a build configuration for a **dynamic library** in **Release** mode, using the **default CMake generator**.

To specify a generatior, add `-G <generator>`, where `<generator>` is one of the supported generators. To see all generators run `cmake --help`.

Currently the CMake support does not introduce any configuration options. It does, however, respect the common CMake ones. 
In particular:
 - To build as static library add `-DBUILD_SHARED_LIBS=OFF`.  
 - To build in Debug add `-DCMAKE_BUILD_TYPE=Debug`

#### Extended Example
The following line, ran from `Source`, configures a **static library** in **Debug**, using the **Ninja** generator in a `./build` subdirectory:
```
cmake -B build -G Ninja -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Debug
```

To build, You can run the build command for the chosen generator. 
```
ninja -C build
```
Or You can use CMake to call the build command for you.
```
cmake --build build
```
In case You are using the Visual Studio generator, You can open the generated solution and not use the command line for the build step.  
Please consult the CMake manual for additional information.


## Implementation Notes
Whenever possible, the CMakeLists.txt files for the depend libraries are the ones from the original project repositories, for the currently used version.  
Some modifications are made, though. These are minor fixups, as well as adding ways to skip configuring install, tests and examples, but also skipping extensive code gen, as FreeImage already comes with all required configurations files.  
Very limited code gen is still present. Specifically, Zlib and LibPNG configuration steps alter the pre-existing config files. Nevertheless, an effort is made to not break the normal, non-CMake builds while using a CMake configuration.
 
-----------------------------------------------------------------------------
What is FreeImage ?
-----------------------------------------------------------------------------
FreeImage is an Open Source library project for developers who would like to support popular graphics image formats like PNG, BMP, JPEG, TIFF and others as needed by today's multimedia applications.
FreeImage is easy to use, fast, multithreading safe, and cross-platform (works with Windows, Linux and Mac OS X).

Thanks to it's ANSI C interface, FreeImage is usable in many languages including C, C++, VB, C#, Delphi, Java and also in common scripting languages such as Perl, Python, PHP, TCL, Lua or Ruby.

The library comes in two versions: a binary DLL distribution that can be linked against any WIN32/WIN64 C/C++ compiler and a source distribution.
Workspace files for Microsoft Visual Studio provided, as well as makefiles for Linux, Mac OS X and other systems.
