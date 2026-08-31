@echo off
setlocal EnableDelayedExpansion
set "ROOT=%~dp0.."
if defined VCToolsInstallDir goto have
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
:have
if not exist "%ROOT%\build\obj\_cc" mkdir "%ROOT%\build\obj\_cc"
if not exist "%ROOT%\build\logs" mkdir "%ROOT%\build\logs"
cl /nologo /std:c17 /W4 /WX /O2 /MT /GS /Gy /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /I"%ROOT%\include" /c /Fo"%ROOT%\build\obj\_cc\\" %* > "%ROOT%\build\logs\cc.log" 2>&1
set "RC=%ERRORLEVEL%"
echo EXITCODE=%RC% >> "%ROOT%\build\logs\cc.log"
exit /b %RC%
