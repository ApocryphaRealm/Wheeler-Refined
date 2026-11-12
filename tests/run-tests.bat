@echo off
setlocal
REM Offline checks - no game needed. Builds each test with the discovered MSVC (find-msvc.bat) into build\tests.
REM context_stack_read_test: the 1.7.1 fix for the AE 1.6.1130+ input-context stack read (InputInject).
REM   run-tests.bat           the fixed reader - exit 0 = pass
REM   run-tests.bat legacy    the 1.7.0 reader on the crash's bytes - faults at 0x67, exit 1 (the repro)

call "%~dp0..\find-msvc.bat"
if errorlevel 1 exit /b 1
set "OUT=%~dp0..\build\tests"
if not exist "%OUT%" mkdir "%OUT%"

cl /nologo /std:c++20 /EHsc /W4 /O1 /Fo"%OUT%\\" /Fe"%OUT%\context_stack_read_test.exe" "%~dp0context_stack_read_test.cpp" >"%OUT%\cl.log"
if errorlevel 1 (
	type "%OUT%\cl.log"
	exit /b 1
)

set "MODE="
if /i "%~1"=="legacy" set "MODE=--legacy"
"%OUT%\context_stack_read_test.exe" %MODE%
exit /b %errorlevel%
