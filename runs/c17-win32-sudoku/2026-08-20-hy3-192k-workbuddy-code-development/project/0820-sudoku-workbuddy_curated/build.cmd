@echo off
setlocal EnableDelayedExpansion
rem ==========================================================================
rem  build.cmd - clean release build of the Sudoku desktop application.
rem  Compiles every translation unit under src/ and links a single GUI
rem  executable (build\bin\sudoku.exe).  Flags match the dev helper _exe.cmd
rem  so a clean result here implies a clean result there (Release Gate G1).
rem ==========================================================================
set "ROOT=%~dp0"

if defined VCToolsInstallDir goto have
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
:have

set "CFLAGS=/nologo /std:c17 /W4 /WX /O2 /Zi /MT /GS /Gy /utf-8 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /I"%ROOT%include""
set "LDFLAGS=/nologo /DEBUG /INCREMENTAL:NO /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT /HIGHENTROPYVA /MACHINE:X64 /SUBSYSTEM:WINDOWS"
set "LDLIBS=kernel32.lib user32.lib gdi32.lib bcrypt.lib"

if not exist "%ROOT%build\obj" mkdir "%ROOT%build\obj"
if not exist "%ROOT%build\bin" mkdir "%ROOT%build\bin"

echo === compiling sources ===
set "FAIL=0"
for /r "%ROOT%src" %%f in (*.c) do (
    echo   compiling %%~nxf
    cl %CFLAGS% /c "%%f" /Fo"%ROOT%build\obj\%%~nf.obj" /Fd"%ROOT%build\obj\%%~nf.pdb" || set "FAIL=1"
)
if "%FAIL%"=="1" (
    echo BUILD FAILED at compile stage
    exit /b 1
)

echo === linking build\bin\sudoku.exe ===
link %LDFLAGS% /OUT:"%ROOT%build\bin\sudoku.exe" /PDB:"%ROOT%build\bin\sudoku.pdb" "%ROOT%build\obj\*.obj" %LDLIBS%
if errorlevel 1 (
    echo BUILD FAILED at link stage
    exit /b 1
)
echo BUILD OK: %ROOT%build\bin\sudoku.exe
exit /b 0
