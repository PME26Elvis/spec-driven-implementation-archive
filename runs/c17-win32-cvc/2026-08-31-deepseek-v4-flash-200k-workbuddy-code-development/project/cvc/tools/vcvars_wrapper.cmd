@echo off
rem Sets up MSVC x64 environment and redirects temp to D: to conserve C: space.
rem Usage: call tools\vcvars_wrapper.cmd <command...>
setlocal DisableDelayedExpansion
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
  echo ERROR: vcvars64.bat not found at %VCVARS% >&2
  exit /b 1
)
rem Redirect MSVC temp/intermediate to D: to avoid consuming C: space.
if not exist "D:\0831-cvc-workbuddy\.cvc_build_tmp" mkdir "D:\0831-cvc-workbuddy\.cvc_build_tmp"
set "TMP=D:\0831-cvc-workbuddy\.cvc_build_tmp"
set "TEMP=D:\0831-cvc-workbuddy\.cvc_build_tmp"
call "%VCVARS%" >nul
if errorlevel 1 exit /b 1
call %*
exit /b %ERRORLEVEL%
