: The following file renaming is needed if one want to compile all files 
: using the same output directory, e.g. "Debug\" or "Release\". 
:
: Usage:
: copy all src WebP files into src\, then, run this script to rename files
:

setlocal

: dec\
pushd "dec\" && for /f "delims=" %%A in ('dir /a-d /b *.c') do (
copy /Y "%%~fA" "%%~dpAdec.%%A"
del /Q "%%~fA"
)
popd

: demux\
pushd "demux\" && for /f "delims=" %%A in ('dir /a-d /b *.c') do (
copy /Y "%%~fA" "%%~dpAdemux.%%A"
del /Q "%%~fA"
)
popd

: dsp\
pushd "dsp\" && for /f "delims=" %%A in ('dir /a-d /b *.c') do (
copy /Y "%%~fA" "%%~dpAdsp.%%A"
del /Q "%%~fA"
)
popd

: enc\
pushd "enc\" && for /f "delims=" %%A in ('dir /a-d /b *.c') do (
copy /Y "%%~fA" "%%~dpAenc.%%A"
del /Q "%%~fA"
)
popd

: mux\
pushd "mux\" && for /f "delims=" %%A in ('dir /a-d /b *.c') do (
copy /Y "%%~fA" "%%~dpAmux.%%A"
del /Q "%%~fA"
)
popd

: utils\
pushd "utils\" && for /f "delims=" %%A in ('dir /a-d /b *.c') do (
copy /Y "%%~fA" "%%~dpAutils.%%A"
del /Q "%%~fA"
)
popd

: webp\
pushd "webp\" && for /f "delims=" %%A in ('dir /a-d /b *.c') do (
copy /Y "%%~fA" "%%~dpAwebp.%%A"
del /Q "%%~fA"
)
popd

endlocal

: Makefiles

del /S /Q Makefile.am
del /S /Q *.pc.in

pause -1


