@echo off
setlocal
REM Build the Skyrim 1.7.x line (configure17.bat runs first when the build directory does not exist yet).

call "%~dp0find-msvc.bat"
if errorlevel 1 exit /b 1

cd /d "%~dp0"
if not exist "build\relwithdebinfo-17\CMakeCache.txt" (
	call "%~dp0configure17.bat"
	if errorlevel 1 exit /b 1
)
cmake --build build\relwithdebinfo-17 --target wheeler
