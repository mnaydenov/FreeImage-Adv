set(src
  decode/decode.c
  decode/JXRTranscode.c
  decode/postprocess.c
  decode/segdec.c
  decode/strdec.c
  decode/strdec_x86.c
  decode/strInvTransform.c
  decode/strPredQuantDec.c
)

list(APPEND src
  encode/encode.c
  encode/segenc.c
  encode/strenc.c
  encode/strenc_x86.c
  encode/strFwdTransform.c
  encode/strPredQuantEnc.c
)

list(APPEND src
  sys/image.c
  sys/perfTimerANSI.c
  sys/strcodec.c
  sys/strPredQuant.c
  sys/strTransform.c
  sys/adapthuff.c
)

add_library(jpegxr STATIC
  ${src}
)

target_compile_definitions(jpegxr PUBLIC ${JXR_DEFINES})
target_include_directories(jpegxr PUBLIC ${JXR_INCLUDES})