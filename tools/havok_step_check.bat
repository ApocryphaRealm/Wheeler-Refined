@echo off
setlocal
REM Compiles tools\havok_step_check.cpp (src\bin\Utilities\HavokStepCore.h: fMaxTime:HAVOK follows the wheel's slow time,
REM so ragdolls near the player do not fly when the wheel opens - 1.5.0) with MSVC and runs it.
call "%~dp0..\find-msvc.bat"
if errorlevel 1 exit /b 1
set OUT=%TEMP%\wheeler-havok-step-check-build
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /std:c++20 /EHsc /O2 /W4 /Fo"%OUT%\\" /Fe"%OUT%\havok_step_check.exe" "%~dp0havok_step_check.cpp" >"%OUT%\cl.log" || (type "%OUT%\cl.log" & exit /b 1)
"%OUT%\havok_step_check.exe" %*
