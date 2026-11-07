@echo off
setlocal
REM Configure the Skyrim 1.7.x build line: CommonLibSSE-NG 7.2.0 from the overlay port in cmake\ports-17, manifest in
REM cmake\manifest-17, into build\relwithdebinfo-17. The first run builds CommonLibSSE-NG and its dependencies through
REM vcpkg, so it takes a while. Requires VCPKG_ROOT; Visual Studio, CMake and Ninja are found by find-msvc.bat.

call "%~dp0find-msvc.bat"
if errorlevel 1 exit /b 1

if "%VCPKG_ROOT%"=="" (
	echo VCPKG_ROOT is not set. Point it at your vcpkg checkout and try again.
	exit /b 1
)

cd /d "%~dp0"
cmake -S . -B build\relwithdebinfo-17 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows-static-md -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DVCPKG_MANIFEST_DIR="%~dp0cmake\manifest-17" -DVCPKG_OVERLAY_PORTS="%~dp0cmake\ports-17" -DCOPY_OUTPUT=OFF -DRUNTIME_LINE=17
