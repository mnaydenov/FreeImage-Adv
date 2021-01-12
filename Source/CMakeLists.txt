cmake_minimum_required(VERSION 3.1)

project(FreeImage VERSION 3.18.1)

set(SKIP_INSTALL_ALL ON)
set(CMAKE_SKIP_INSTALL_RULES ON) 
set(SKIP_BUILD_EXAMPLES ON)            # For Zlib
set(SKIP_BUILD_TESTS ON)               # Ours
set(SKIP_CODEGEN ON)                   # Ours. 

add_subdirectory(ZLib)

set(ZLIB_LIBRARY zlib)
set(ZLIB_FOUND   TRUE)
set(ZLIB_LIBRARIES    ${ZLIB_LIBRARY})
set(ZLIB_INCLUDE_DIR ${zlib_SOURCE_DIR})
set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR} ${zlib_BINARY_DIR})

function(option_enable var)
  set(${var} ON CACHE BOOL "")
endfunction()
function(option_disable var)
  set(${var} OFF CACHE BOOL "")
endfunction()

option_enable(PNG_BUILD_ZLIB)
option_disable(PNG_SHARED)
option_enable(PNG_STATIC)
option_disable(PNG_TESTS)

add_subdirectory(LibPNG)

option_disable(BUILD_SHARED_LIBS)
option_disable(NAMESPACE_VERSIONING)

add_subdirectory(OpenEXR)           # Corresponds to IlmBase
add_subdirectory(OpenEXR/IlmImf)    
