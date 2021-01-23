cmake_minimum_required(VERSION 3.12)    #< Highest, used by sub-projects (LibJXR, LibRaw)

project(FreeImage VERSION 3.18.1)

set(CMAKE_SKIP_INSTALL_RULES ON)       #< TODO Handle install
set(SKIP_INSTALL_ALL ON)               #< 

set(SKIP_BUILD_TESTS ON)               #< Extra, added to some sub-projects
set(SKIP_CODEGEN ON)                   #< 

set(BUILD_SHARED_LIBS ON)              #< Default to shared library

if(NOT $CACHE{BUILD_SHARED_LIBS} STREQUAL "")        #< cmake(3.14): use DEFINED CACHE{BUILD_SHARED_LIBS}  
  set(BUILD_SHARED_LIBS $CACHE{BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS_MSG "-- User-selected BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")
else()
  set(BUILD_SHARED_LIBS_MSG "-- Default BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")
endif()

set(OLD_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})  
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)       #< All sub-projects are static

# --- helpers

function(option_enable var)
  set(${var} ON CACHE BOOL "")
endfunction()
function(option_disable var)
  set(${var} OFF CACHE BOOL "")
endfunction()

message(
  "
  #
  # ---------------- ZLib ---------------------
  #
  "
)

set(SKIP_BUILD_EXAMPLES ON)           

add_subdirectory(ZLib)

set(ZLIB_LIBRARY zlib)
set(ZLIB_FOUND   TRUE)
set(ZLIB_LIBRARIES    ${ZLIB_LIBRARY})
set(ZLIB_INCLUDE_DIR ${zlib_SOURCE_DIR} CACHE PATH "")      # CACHE needed
set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR} ${zlib_BINARY_DIR})

target_include_directories(zlib INTERFACE ${ZLIB_INCLUDE_DIRS})   #< zlib only sets this, upon installation. 
                                                                  # (MSVC:fails, Ninja:OK) 

message(
  "
  #
  # ---------------- LibJPEG ------------------
  #
  "
)

add_subdirectory(LibJPEG)

# ### This is trying to override what FindJPEG.cmake does.  
# LibJpeg is used by both LibTiff and LibRaw, in both cases via find_package(JPEG) 

set(JPEG_FOUND   TRUE)
set(JPEG_LIBRARY jpeg)                       
set(JPEG_INCLUDE_DIR  ${jpeg_SOURCE_DIR} CACHE PATH "")    # CACHE needed
set(JPEG_LIBRARIES    ${JPEG_LIBRARY})
set(JPEG_INCLUDE_DIRS ${JPEG_INCLUDE_DIR})
# # JPEG::JPEG target is already present as an alias in our LibJPEG

# ### ZLib and LibJPEG must be first, because some sub-projects mess with their *_INCLUDE_DIR variables

message(
  "
  #
  # ---------------- LibPNG --------------------
  #
  "
)

option_enable(PNG_BUILD_ZLIB)
option_disable(PNG_SHARED)
option_enable(PNG_STATIC)
option_disable(PNG_TESTS)

add_subdirectory(LibPNG)

message(
  "
  #
  # ---------------- OpenEXR -------------------
  #
  "
)

option_disable(NAMESPACE_VERSIONING)   

add_subdirectory(OpenEXR)                #< Corresponds to IlmBase
add_subdirectory(OpenEXR/IlmImf)    


message(
  "
  #
  # ---------------- LibWebP ------------------
  #
  "
)

option_disable(WEBP_BUILD_ANIM_UTILS)    
option_disable(WEBP_BUILD_CWEBP)  
option_disable(WEBP_BUILD_DWEBP)  
option_disable(WEBP_BUILD_GIF2WEBP)  
option_disable(WEBP_BUILD_IMG2WEBP)   
option_disable(WEBP_BUILD_VWEBP)   
option_disable(WEBP_BUILD_WEBPINFO)   
option_disable(WEBP_BUILD_WEBPMUX)   
option_disable(WEBP_BUILD_EXTRAS)   

set(WEBP_BUILD_LIBWEBPMUX ON)            #< Extra, added to avid building GIF2WEBP and/or IMG2WEBP

add_subdirectory(LibWebP)

message(
  "
  #
  # ---------------- LibRaw ------------------
  #
  "
)

option_disable(ENABLE_LCMS)

add_subdirectory(LibRawLite)

message(
  "
  #
  # ---------------- LibTiff ------------------
  #
  "
)

set(CXX_SUPPORT OFF)

# ### What follows is verbatim from the main CMakeLists.txt of the LibTiff(4.1.0) project 

macro(extra_dist)
  foreach(file ${ARGV})
    file(RELATIVE_PATH relfile "${PROJECT_SOURCE_DIR}"
         "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
    list(APPEND EXTRA_DIST "${relfile}")
  endforeach()
  set(EXTRA_DIST "${EXTRA_DIST}" PARENT_SCOPE)
endmacro()

list(APPEND TIFF_INCLUDES ${ZLIB_INCLUDE_DIRS})
list(APPEND TIFF_INCLUDES ${JPEG_INCLUDE_DIR})

list(APPEND TIFF_LIBRARY_DEPS ${ZLIB_LIBRARIES})
list(APPEND TIFF_LIBRARY_DEPS ${JPEG_LIBRARIES})

add_subdirectory(LibTIFF4)

message(
  "
  #
  # ---------------- LibOpenJPEG ------------------
  #
  "
)

# ### What follows is verbatim from the main CMakeLists.txt of the LibOpenJPEG(2.1.2) project 

set(OPENJPEG_LIBRARY_NAME openjp2)

set(OPENJPEG_VERSION_MAJOR 2)
set(OPENJPEG_VERSION_MINOR 1)
set(OPENJPEG_VERSION_BUILD 2)
set(OPENJPEG_VERSION
  "${OPENJPEG_VERSION_MAJOR}.${OPENJPEG_VERSION_MINOR}.${OPENJPEG_VERSION_BUILD}")
set(PACKAGE_VERSION
  "${OPENJPEG_VERSION_MAJOR}.${OPENJPEG_VERSION_MINOR}.${OPENJPEG_VERSION_BUILD}")

set(OPENJPEG_SOVERSION 7)

set(OPENJPEG_LIBRARY_PROPERTIES
  VERSION   "${OPENJPEG_VERSION_MAJOR}.${OPENJPEG_VERSION_MINOR}.${OPENJPEG_VERSION_BUILD}"
  SOVERSION "${OPENJPEG_SOVERSION}"
)

if(CMAKE_COMPILER_IS_GNUCC)
  # For all builds, make sure openjpeg is std99 compliant:
  # set(CMAKE_C_FLAGS "-Wall -std=c99 ${CMAKE_C_FLAGS}") # FIXME: this setting prevented us from setting a coverage build.
  # Do not use ffast-math for all build, it would produce incorrect results, only set for release:
  set(OPENJPEG_LIBRARY_COMPILE_OPTIONS ${OPENJPEG_LIBRARY_COMPILE_OPTIONS} "$<$<CONFIG:Release>:-ffast-math>")
endif()

add_subdirectory(LibOpenJPEG)

message(
  "  
  #
  # ------------------ LibJXR --------------------
  #
  "
)

add_subdirectory(LibJXR)

message(
  "  # ---------------- FreeImage ------------------
  "
)

message(${BUILD_SHARED_LIBS_MSG})
  
set(BUILD_SHARED_LIBS ${OLD_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)

set(freeimage_src
  "FreeImage/PluginPNG.cpp"
  "FreeImage/PluginPNM.cpp"
  "FreeImage/PluginPSD.cpp"
  "FreeImage/PluginRAS.cpp"
  "FreeImage/PluginRAW.cpp"
  "FreeImage/PluginSGI.cpp"
  "FreeImage/PluginTARGA.cpp"
  "FreeImage/PluginTIFF.cpp"
  "FreeImage/PluginWBMP.cpp"
  "FreeImage/PluginWebP.cpp"
  "FreeImage/PluginXBM.cpp"
  "FreeImage/PluginXPM.cpp"
  "FreeImage/PSDParser.cpp"
  "FreeImage/PSDParser.h"
  "FreeImage/TIFFLogLuv.cpp"
  "FreeImage/tmoColorConvert.cpp"
  "FreeImage/tmoDrago03.cpp"
  "FreeImage/tmoFattal02.cpp"
  "FreeImage/tmoReinhard05.cpp"
  "FreeImage/ToneMapping.cpp"
  "FreeImage/WuQuantizer.cpp"
  "FreeImage/ZLibInterface.cpp"
  "FreeImage/BitmapAccess.cpp"
  "FreeImage/CacheFile.cpp"
  "FreeImage/ColorLookup.cpp"
  "FreeImage/Conversion.cpp"
  "FreeImage/Conversion4.cpp"
  "FreeImage/Conversion8.cpp"
  "FreeImage/Conversion16_555.cpp"
  "FreeImage/Conversion16_565.cpp"
  "FreeImage/Conversion24.cpp"
  "FreeImage/Conversion32.cpp"
  "FreeImage/ConversionFloat.cpp"
  "FreeImage/ConversionRGB16.cpp"
  "FreeImage/ConversionRGBA16.cpp"
  "FreeImage/ConversionRGBAF.cpp"
  "FreeImage/ConversionRGBF.cpp"
  "FreeImage/ConversionType.cpp"
  "FreeImage/ConversionUINT16.cpp"
  "FreeImage/FreeImage.cpp"
  "FreeImage/FreeImageC.c"
  "FreeImage/FreeImageIO.cpp"
  "FreeImage/GetType.cpp"
  "FreeImage/Halftoning.cpp"
  "FreeImage/J2KHelper.cpp"
  "FreeImage/J2KHelper.h"
  "FreeImage/LFPQuantizer.cpp"
  "FreeImage/MemoryIO.cpp"
  "FreeImage/MNGHelper.cpp"
  "FreeImage/MultiPage.cpp"
  "FreeImage/NNQuantizer.cpp"
  "FreeImage/PixelAccess.cpp"
  "FreeImage/Plugin.cpp"
  "FreeImage/PluginBMP.cpp"
  "FreeImage/PluginCUT.cpp"
  "FreeImage/PluginDDS.cpp"
  "FreeImage/PluginEXR.cpp"
  "FreeImage/PluginG3.cpp"
  "FreeImage/PluginGIF.cpp"
  "FreeImage/PluginHDR.cpp"
  "FreeImage/PluginICO.cpp"
  "FreeImage/PluginIFF.cpp"
  "FreeImage/PluginJ2K.cpp"
  "FreeImage/PluginJNG.cpp"
  "FreeImage/PluginJP2.cpp"
  "FreeImage/PluginJPEG.cpp"
  "FreeImage/PluginJXR.cpp"
  "FreeImage/PluginKOALA.cpp"
  "FreeImage/PluginMNG.cpp"
  "FreeImage/PluginPCD.cpp"
  "FreeImage/PluginPCX.cpp"
  "FreeImage/PluginPFM.cpp"
  "FreeImage/PluginPICT.cpp"
)

set(freeimagetoolkit_src
  "FreeImageToolkit/Flip.cpp"
  "FreeImageToolkit/JPEGTransform.cpp"
  "FreeImageToolkit/MultigridPoissonSolver.cpp"
  "FreeImageToolkit/Rescale.cpp"
  "FreeImageToolkit/Resize.cpp"
  "FreeImageToolkit/Resize.h"
  "FreeImageToolkit/Background.cpp"
  "FreeImageToolkit/BSplineRotate.cpp"
  "FreeImageToolkit/Channels.cpp"
  "FreeImageToolkit/ClassicRotate.cpp"
  "FreeImageToolkit/Colors.cpp"
  "FreeImageToolkit/CopyPaste.cpp"
  "FreeImageToolkit/Display.cpp"
  "FreeImageToolkit/Filters.h"
)

set(freeimagemetadata_src
  "Metadata/FreeImageTag.h"
  "Metadata/IPTC.cpp"
  "Metadata/TagConversion.cpp"
  "Metadata/TagLib.cpp"
  "Metadata/XTIFF.cpp"
  "Metadata/Exif.cpp"
  "Metadata/FIRational.cpp"
  "Metadata/FIRational.h"
  "Metadata/FreeImageTag.cpp"
)

set(jpegtransform_src    #< Yes, we are naughty, but Lossless Jpeg Transformations are valuable 
  "LibJPEG/transupp.c"   #< TODO: Investigate/negotiate these to be officially part of LibJpeg/JpegTurbo/Mozjpeg 
)

add_library(freeimage 
  ${freeimage_src}
  ${freeimagetoolkit_src}
  ${freeimagemetadata_src}
  ${jpegtransform_src}
)

if(${BUILD_SHARED_LIBS})
 target_compile_definitions(freeimage PUBLIC FREEIMAGE_EXPORTS)
else()
 target_compile_definitions(freeimage PUBLIC FREEIMAGE_LIB)
endif()

target_compile_definitions(freeimage PRIVATE OPJ_STATIC)  # ### else __declspec(dllimport) will be defined, failing to link (mingw-w64)

target_include_directories(freeimage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(freeimage 
  zlib
  jpeg
  png_static 
  webp
  libwebpmux
  raw
  tiff
  openjp2
  IlmImf
  jxr
)

target_include_directories(freeimage PRIVATE 
  ${ZLIB_INCLUDE_DIRS}
  ${libpng_BINARY_DIR}
  ${ilmbase_BINARY_DIR}/config # ### PRIVATE in OpenEXR, yet "ImathNamespace.h" includes "IlmBaseConfig.h" ?!?
)

# ### Make sure we have zconf.h in zlib sources as FreeImage regular build system depends on that

if((NOT EXISTS ZLib/zconf.h.old) AND EXISTS ZLib/zconf.h.included)   #< zlib renames existing zconf.h to zconf.h.included,
  file(RENAME ZLib/zconf.h.included ZLib/zconf.h.old)                # preserve the very first zconf.h
endif()

if((NOT EXISTS ZLib/zconf.h) AND EXISTS ${zlib_BINARY_DIR}/zconf.h)                    #< if zlib is already build, 
  file(COPY ${zlib_BINARY_DIR}/zconf.h DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/ZLib)   # we have the new zconf.h already
endif()

add_custom_command(TARGET freeimage                               #< # Sadly, we can't assign to zlib
                  POST_BUILD
                  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${zlib_BINARY_DIR}/zconf.h ${CMAKE_CURRENT_SOURCE_DIR}/ZLib
                  )

message("------------------------------------------------")
