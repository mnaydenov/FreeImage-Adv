cmake_minimum_required(VERSION 3.12)  #< needed target link object libraries

project(jxr VERSION 1.1)

include(TestBigEndian)
TEST_BIG_ENDIAN(BIG_ENDIAN)

set(JXR_DEFINES 
  DISABLE_PERF_MEASUREMENT
  )

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")   #< MSVC will use x86.h
  list(APPEND JXR_DEFINES
    __ANSI__                                    #< ansi.h
  )   
endif()

if(BIG_ENDIAN)
  list(APPEND JXR_DEFINES 
    _BIG__ENDIAN_
  )
endif()

set(JXR_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include
  ${CMAKE_CURRENT_SOURCE_DIR}/image/sys
)          

add_subdirectory(image)
add_subdirectory(jxrgluelib)

add_library(jxr)

target_link_libraries(jxr PUBLIC jpegxr jxrglue)