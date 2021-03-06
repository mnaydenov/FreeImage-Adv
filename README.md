## Branches of interest

 - `callbacks`: (WIP) Same as `RAII` + callbacks support, allowing progress monitoring and cancelation (partial).
 - `RAII`: (WIP) Same as `master` + RAII patches for memory management.
 - `master`: Same as `master-sourceforge-cmake` + ASan patches.
 - `master-sourceforge-cmake`: Same as `master-sourceforge` + CMake support.
 - `master-sourceforge`: Clone of https://sourceforge.net/projects/freeimage/ (trunk Jan. 2021)
---
# Callbacks
## What is changed
 - Added new `FreeImageCB` struct, containing callback functions for start, progress, finish and message output for an operation.
 - Added new `FreeImageLoadArgs` struct, serving as a more extensible way of passing arguments to the loading functions.

## How to use it
> For now only the following loading functions have callback support: JPEG, PNG, BPM, TIFF, TARGA, PSD, RAW-Preview, WebP, EXR, HDR

The new functionality is exposed via new public functions that take an `FreeImageLoadArgs` instead of the old `int flags` argument:
```
FIBITMAP * FreeImage_LoadAdv(FREE_IMAGE_FORMAT fif, const char *filename, const FreeImageLoadArgs* args);
```
_Versions for loading from handle and memory, as well as supporting `wchar` filenames are also available._   

`FreeImageLoadArgs` contains all loading options, along side the new `FreeImageCB` struct:  
```
struct FreeImageLoadArgs {
  unsigned flags;      //< lower 16 bits: same as old flags argument
  unsigned option;     //< lower 16 bits: FIF_JPEG: desired downscale size
  
  unsigned cbOption;   //< lower 8 bits: number of times onProgress should be called while loading
  const FreeImageCB* cb;
  
  void* more;
};
```
 - `flags` is equivalent to old flags argument with the notable exception of passing integer values (like downscale size). These are handled by a separate `option` member.
 - `option` is an integer value, the loading function might accept. Currently only JPEG loading uses this for passing downscale size. 
 - `cbOption` controls how the callback is used. Currently it is interpreted as the desired number of times `FreeImageCB::onProgress` is invoked. 
 - `cb` holds the actual callbacks.

`FreeImageCB` contains the callbacks as function pointers, as well as a pointer to user-data: 
```
struct FreeImageCB {
  void* user;
  FI_OnStartedProc onStarted;
  FI_OnProgressProc onProgress;
  FI_OnFinishedProc onFinished;
  FI_OnMessageProc onMessage;

  void* more;
};
```
These are all (optionally) supplied by the user in order to have feedback about the state of operation.  
For example, here is how to create a simple setup to monitor start and finish of image loading:
```
// prepare the callbacks

DLL_CALLCONV BOOL OnStartedProc(void* user, FREE_IMAGE_OPERATION operation, unsigned which, void*) {
  // signal "started" 

  return true; //< do continue with the operation
}
DLL_CALLCONV void OnFinishedProc(void* user, const BOOL* success, void*) {
  // signal "finished"
}

int main() {
  // init FreeImage if needed

  // setup callbacks

  FreeImageCB cb{};
  cb.onStarted = &OnStartedProc;
  cb.onFinished = &OnFinishedProc;

  // setup loading argument

  FreeImageLoadArgs args{};
  args.cb = &cb;

  // load the image

  auto* dib = FreeImage_LoadAdv(FIF_JPEG, "some-path/image.jpg", &args);

  //  use the dib

  // de-init FreeImage if needed

  return 0;
}
```
In the example above, the user-supplied `OnStartedProc` will be called just before actual load of image data starts. The user has the option to cancel loading at that point by returning `false`. On the other hand `OnFinishedProc` will be called just after the all data from the image is either loaded, an error occurred or the operation is canceled.  
All currently available callbacks are as follows:
```
BOOL (*FI_OnStartedProc) (void* user, FREE_IMAGE_OPERATION operation, unsigned which, void* more);
```
This one is called when an operation has started - the input is successfully opened and data is about to be read. 
 - `user` has the value of the `FreeImageCB::user` member, supplied when the struct is created.
 - `operation` is a new enum denoting different operations, executed by FreeImage, like loading, saving, rotation, tone-mapping etc. Currently callbacks are implemented only in loading and `FI_OP_LOAD` will be the only value `operation` will have.
 - `which` depends on the `operation` and denotes the type of operation. For `FI_OP_LOAD` (the only implemented operation) this is the `FREE_IMAGE_FORMAT` of the started loading.  
 - **return** `TRUE` to continue with the operation; `FALSE` to cancel. If canceled, loading functions return null. 
```
BOOL (*FI_OnProgressProc)(void* user, double progress, void* more);
```
This  is called while the operation is in progress. In the case of image loading, when it is called is controlled by the `FreeImageLoadArgs::cbOption` member. At the moment only the first 8 bits ot this member are used and are interpreted as an simple count of how many times this callback is to be invoked (up 255 times). 
In future more options could be envisioned like number of lines skipped or number of milliseconds passed.
>Note that currently this count is just a hint! The end result may vary greatly.
 - `user` has the value of the `FreeImageCB::user` member, supplied when the struct is created.
 - `progress` argument ranges from `(0, 1]` indicating the progress made. 
 >Note that the final value(s) of `progress` might not be reached and that there will be uneven jumps b/w received values. This argument is only to have some overall visual feedback, not to give detailed information. In general, the primary purpose of the progress callback is to have cancellation support.
 - **return** `TRUE` to continue with the operation; `FALSE` to cancel. If canceled, loading functions return null.
```
void (*FI_OnFinishedProc)(void* user, const BOOL* success, void* more);
```
When the operation completes, no matter if successfully or not, this callback is invoked.
 - `user` has the value of the `FreeImageCB::user` member, supplied when the struct is created.
 - `success` argument indicates in what state the operation completes. If the argument is null, the operation has been canceled. Otherwise its value denotes success (`TRUE`) or failure (`FALSE`) in the case of error.
```
void (*FI_OnMessageProc) (void* user, const char* msg, void* more);
```
The last callback is an improved version of the existing `FreeImage_OutputMessageFunction`. It will receive the same messages as the old procedure, however it allows dedicated messages (in contrast of having a global function), as well as passing user-data.  

>Note, all `more` arguments (and members) are reserved for future expansion. Reserved are also all unused bits in the `FreeImageLoadArgs` members.


---
# CMake support
## What is changed ?

 - Added CMake support for the base library and its dependencies.
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
Some modifications are made, though. These are minor fixups, as well as adding ways to skip configuring install, tests and examples, but also skipping extensive code gen.  

>NOTE: After CMake runs the following original source files will be renamed:  
>`ZLib/zconf.h => ZLib/zconf.h.included` 
>`LibPNG/pnglibconf.h => LibPNG/pnglibconf.h.old`  
>If you want to compile the original Visual Studio solution after that, remember to restore them back.
 
-----------------------------------------------------------------------------
# FreeImage 
What is FreeImage ?
-----------------------------------------------------------------------------
FreeImage is an Open Source library project for developers who would like to support popular graphics image formats like PNG, BMP, JPEG, TIFF and others as needed by today's multimedia applications.
FreeImage is easy to use, fast, multithreading safe, and cross-platform (works with Windows, Linux and Mac OS X).

Thanks to it's ANSI C interface, FreeImage is usable in many languages including C, C++, VB, C#, Delphi, Java and also in common scripting languages such as Perl, Python, PHP, TCL, Lua or Ruby.

The library comes in two versions: a binary DLL distribution that can be linked against any WIN32/WIN64 C/C++ compiler and a source distribution.
Workspace files for Microsoft Visual Studio provided, as well as makefiles for Linux, Mac OS X and other systems.
