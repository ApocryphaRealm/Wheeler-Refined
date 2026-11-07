@echo off
setlocal
REM Build the plugin. The toolchain is DISCOVERED, never hardcoded (rule 4): find-msvc.bat locates
REM Visual Studio with vswhere, enters its x64 developer environment and puts the CMake and Ninja it
REM ships with on PATH.
REM
REM 1.2.0: this file used to be a bare `cmake --preset vs2022-windows`. That failed twice over on a
REM machine whose Visual Studio has moved on - it put no cmake on PATH at all, and the preset pins
REM the "Visual Studio 17 2022" generator with toolset v143, neither of which exists here any more
REM (Visual Studio 2026 / toolset 14.51). The preset is left untouched for anyone still on 2022;
REM this script configures the same build - same vcpkg toolchain, same static-md triplet, Release,
REM no deploy copy - through Ninja, which every Visual Studio since 2019 ships.

call "%~dp0find-msvc.bat"
if errorlevel 1 exit /b 1

cd /d "%~dp0"

if "%VCPKG_ROOT%"=="" (
	echo VCPKG_ROOT is not set. Point it at your vcpkg checkout and try again.
	exit /b 1
)
if "%CommonLibSSEPath_NG%"=="" set "CommonLibSSEPath_NG=C:\src\CommonLibSSE-NG"
if not exist "%CommonLibSSEPath_NG%" (
	echo CommonLibSSE-NG was not found at "%CommonLibSSEPath_NG%".
	echo Set CommonLibSSEPath_NG to your checkout and try again.
	exit /b 1
)

REM The SE 1.5.97 / AE 1.6.x line (RUNTIME_LINE 1). Its tree is build\relwithdebinfo-se-only, the name package-mod
REM reads beside build\relwithdebinfo-17 (the Skyrim 1.7.x line: configure17.bat / build17.bat).
if not exist "build\relwithdebinfo-se-only\CMakeCache.txt" (
	cmake -S . -B build\relwithdebinfo-se-only -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DCOPY_OUTPUT=OFF -DRUNTIME_LINE=1 -DCommonLibSSEPath_NG="%CommonLibSSEPath_NG%"
	if errorlevel 1 exit /b 1
)
cmake --build build\relwithdebinfo-se-only --target wheeler
