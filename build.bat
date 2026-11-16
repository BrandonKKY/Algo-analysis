@echo off
rem Builds greedy seam carving with Visual Studio's C++ compiler (MSVC).
rem Works from any terminal: it locates Visual Studio and sets up the compiler itself.
rem   build.bat         build build\greedy_carve.exe and build\test_greedy.exe
rem   build.bat test    build, then run the unit tests
setlocal
cd /d "%~dp0"

where cl >nul 2>nul
if errorlevel 1 (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
        echo error: Visual Studio not found. Install "Desktop development with C++".
        exit /b 1
    )
    for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
)
if defined VSDIR (
    if not exist "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" (
        echo error: Visual Studio C++ tools not found. Install "Desktop development with C++".
        exit /b 1
    )
    call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
)

if not exist build mkdir build
set FLAGS=/nologo /std:c++17 /O2 /EHsc /W4 /Isrc /Ithird_party /Fobuild\
set LIB_SRC=src\image.cpp src\energy.cpp src\greedy.cpp

cl %FLAGS% %LIB_SRC% src\main.cpp /Fe:build\greedy_carve.exe || exit /b 1
cl %FLAGS% %LIB_SRC% tests\test_greedy.cpp /Fe:build\test_greedy.exe || exit /b 1
echo.
echo Built build\greedy_carve.exe and build\test_greedy.exe

if /i "%~1"=="test" build\test_greedy.exe
