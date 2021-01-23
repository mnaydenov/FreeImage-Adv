set(src
  JXRGlueJxr.c
  JXRGluePFC.c
  JXRMeta.c
  JXRGlue.c
)

add_library(jxrglue OBJECT 
  ${src}
)

target_compile_definitions(jxrglue PUBLIC ${JXR_DEFINES})
target_include_directories(jxrglue PUBLIC ${JXR_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR})
