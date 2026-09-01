# build.ps1 - Build cvc.exe and test harness using MSVC C17.
# Usage: powershell -ExecutionPolicy Bypass -File tools\build.ps1
# Redirects all MSVC temp/intermediate output to D: to conserve C: space.

param(
    [switch]$Clean,
    [switch]$Tests
)

$ErrorActionPreference = "Stop"
$ProjectRoot = "D:\0831-cvc-workbuddy\cvc"
$BuildTmp   = "D:\0831-cvc-workbuddy\.cvc_build_tmp"
$ObjDir     = Join-Path $BuildTmp "obj"
$SrcDir     = Join-Path $ProjectRoot "src"

# Redirect temp to D: to avoid C: usage.
if (-not (Test-Path $BuildTmp)) { New-Item -ItemType Directory -Path $BuildTmp | Out-Null }
$env:TMP = $BuildTmp
$env:TEMP = $BuildTmp

# Locate MSVC toolchain.
$VCVars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $VCVars)) { Write-Error "vcvars64.bat not found"; exit 1 }

# Load MSVC env by running vcvars and capturing environment.
$env:VCINSTALLDIR = $null
cmd /c "`"$VCVars`" >nul 2>nul && set" | ForEach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
        Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
    }
}

# Pick up MSVC's cl. After vcvars, cl is on PATH.
$cl = (Get-Command cl.exe -ErrorAction SilentlyContinue)
if (-not $cl) { Write-Error "cl.exe not on PATH after vcvars"; exit 1 }

if (-not (Test-Path $ObjDir)) { New-Item -ItemType Directory -Path $ObjDir | Out-Null }

# Source files (exclude main.c when building the test harness? keep main separate).
# cvc.exe = all src + main.c
$Common = @(
    "util.c","sha256.c","utf8.c","json.c","glob.c","diff.c",
    "win32.c","repo.c","objects.c","scan.c","merge.c","verify.c","cli.c"
)

function Invoke-Cl {
    param([string]$Args)
    & cmd /c "$cl $Args" 2>&1
    if ($LASTEXITCODE -ne 0) { Write-Error "cl failed: $Args" }
}

Write-Host "== Building cvc.exe =="
$srcList = ($Common | ForEach-Object { "`"$SrcDir\$_`"" }) -join " "
Invoke-Cl "/nologo /std:c17 /W4 /Fo`"$ObjDir\\`" /Fe`"$BuildTmp\cvc.exe`" $srcList `"$SrcDir\main.c`" /link"

Write-Host "== Build OK: $BuildTmp\cvc.exe =="
