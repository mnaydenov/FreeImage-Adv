# yue.nicholas@gmail.com

IF(BUILD_SHARED_LIBS)
    ADD_DEFINITIONS(-DIEX_EXPORTS)
ENDIF()

ADD_LIBRARY ( Iex ${LIB_TYPE}
  IexBaseExc.cpp
  IexThrowErrnoExc.cpp
  )
  
target_include_directories(Iex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT SKIP_INSTALL_ALL)
  INSTALL ( TARGETS
    Iex
    DESTINATION
    lib
  )

  INSTALL ( FILES 
    IexBaseExc.h
    IexMathExc.h
    IexThrowErrnoExc.h
    IexErrnoExc.h
    IexMacros.h
    Iex.h
    IexNamespace.h
    IexExport.h
    IexForward.h
    DESTINATION
    include/OpenEXR
  )
endif()

