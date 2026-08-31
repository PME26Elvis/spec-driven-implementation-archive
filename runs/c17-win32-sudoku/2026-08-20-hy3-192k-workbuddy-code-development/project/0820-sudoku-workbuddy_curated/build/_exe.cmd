@echo off
setlocal EnableDelayedExpansion
rem ==========================================================================
rem  _exe.cmd <outname> <source-or-wildcard> [...]
rem
rem  Incremental development helper: compiles the listed translation units and
rem  links a console executable into build\bin.  Uses exactly the same flags as
rem  build.cmd so a clean result here implies a clean result there.
rem  All compiler / linker output is captured into build\logs\<outname>.log.
rem ==========================================================================
set "ROOT=%~dp0.."
set "BUILD=%ROOT%\build"
set "OBJ=%BUILD%\obj"
set "BIN=%BUILD%\bin"
set "LOGDIR=%BUILD%\logs"

if defined VCToolsInstallDir goto have
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
:have

set "NAME=%~1"
shift
if "%NAME%"=="" (echo usage: _exe.cmd ^<outname^> ^<sources...^> & exit /b 2)

if not exist "%OBJ%\%NAME%" mkdir "%OBJ%\%NAME%"
if not exist "%BIN%" mkdir "%BIN%"
if not exist "%LOGDIR%" mkdir "%LOGDIR%"
del /q "%OBJ%\%NAME%\*.obj" >nul 2>&1

set "SRCS="
:collect
if "%~1"=="" goto run
set "SRCS=!SRCS! "%~1""
shift
goto collect

:run
set "CFLAGS=/nologo /std:c17 /W4 /WX /O2 /Zi /MT /GS /Gy /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /I"%ROOT%\include""
set "LDFLAGS=/nologo /DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /MACHINE:X64"
set "LDLIBS=kernel32.lib user32.lib gdi32.lib bcrypt.lib"

> "%LOGDIR%\%NAME%.log" echo === compile %NAME% ===
cl %CFLAGS% /c /Fo"%OBJ%\%NAME%\\" /Fd"%OBJ%\%NAME%\%NAME%.pdb"!SRCS! >> "%LOGDIR%\%NAME%.log" 2>&1
if errorlevel 1 (
  echo CC_EXITCODE=1 >> "%LOGDIR%\%NAME%.log"
  echo [_exe] compile failed - see build\logs\%NAME%.log
  exit /b 1
)
>> "%LOGDIR%\%NAME%.log" echo === link %NAME% ===
link %LDFLAGS% /SUBSYSTEM:CONSOLE /OUT:"%BIN%\%NAME%.exe" /PDB:"%BIN%\%NAME%.pdb" "%OBJ%\%NAME%\*.obj" %LDLIBS% >> "%LOGDIR%\%NAME%.log" 2>&1
if errorlevel 1 (
  echo LINK_EXITCODE=1 >> "%LOGDIR%\%NAME%.log"
  echo [_exe] link failed - see build\logs\%NAME%.log
  exit /b 1
)
echo EXITCODE=0 >> "%LOGDIR%\%NAME%.log"
echo [_exe] ok: %BIN%\%NAME%.exe
exit /b 0
