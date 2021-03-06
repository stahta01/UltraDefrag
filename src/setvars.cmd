@echo off
echo Set common environment variables...

:: UltraDefrag version
set VERSION_MAJOR=7
set VERSION_MINOR=0
set VERSION_REVISION=4

:: alpha1, beta2, rc3, etc.
:: unset for the final releases
set RELEASE_STAGE=alpha1

:: paths to development tools
set WXWIDGETS=A:\wxWidgets
set WXWIDGETSDIR=%WXWIDGETS%
set WINSDKBASE=C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A
set MINGWBASE=C:\msys64\mingw32
set MINGWx64BASE=C:\msys64\mingw64
set NSISDIR=C:\Program Files (x86)\NSIS
set SEVENZIP_PATH=C:\Program Files (x86)\7-Zip\
set GNUWIN32_DIR=%MINGWx64BASE%\bin
set CODEBLOCKS_EXE=C:\Program Files (x86)\CodeBlocks\codeblocks.exe

:: auxiliary stuff
set VERSION=%VERSION_MAJOR%,%VERSION_MINOR%,%VERSION_REVISION%,0
set VERSION2="%VERSION_MAJOR%, %VERSION_MINOR%, %VERSION_REVISION%, 0\0"
set ULTRADFGVER=%VERSION_MAJOR%.%VERSION_MINOR%.%VERSION_REVISION%
if "%RELEASE_STAGE%" neq "" (
    set UDVERSION_SUFFIX=%ULTRADFGVER%-%RELEASE_STAGE%
) else (
    set UDVERSION_SUFFIX=%ULTRADFGVER%
)

:: Temporary vars for quickmanualbuildscript
set WX_CONFIG=winsdk-amd64
::set WX_GCC_CPPFLAGS=-g0 -Wno-deprecated -m64
set WXWIDGETS_LIB_PATH=%WXWIDGETS%\lib\vc_x64_lib%WX_CONFIG%
set WXWIDGETS_INC2_PATH=%WXWIDGETS_LIB_PATH%\mswu
set WXWIDGETS_INC_PATH=%WXWIDGETS%\include
:: Command to build:
::build --use-winsdk --no-x86 --no-ia64